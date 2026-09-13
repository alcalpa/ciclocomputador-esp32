#include "gps_task.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "minmea.h"
#include "pins.h"
#include "shared_state.h"

static const char *TAG = "gps_task";

#define GPS_UART_PUERTO             UART_NUM_1
#define GPS_UART_BAUDIOS_FABRICA    9600    /* velocidad con la que arranca el NEO-M8N */
#define GPS_UART_BAUDIOS            115200  /* velocidad que se le pide por UBX al arrancar */
#define GPS_FRECUENCIA_HZ           5
#define GPS_UART_BUFFER_RX          2048
#define GPS_UART_TAM_COLA_EVENTOS   16
#define GPS_TAM_TRAMA               (MINMEA_MAX_SENTENCE_LENGTH + 4)
#define GPS_TIMEOUT_SIN_DATOS_MS    3000
#define GPS_NUDOS_A_KMH             1.852f

/* Protocolo binario UBX de u-blox: solo lo justo para configurar el modulo. */
#define UBX_SYNC_1                  0xB5
#define UBX_SYNC_2                  0x62
#define UBX_CLASE_CFG               0x06
#define UBX_ID_CFG_PRT              0x00
#define UBX_ID_CFG_MSG              0x01
#define UBX_ID_CFG_RATE             0x08
#define UBX_CARGA_MAX               20

/* Identificadores de las tramas NMEA estandar dentro de UBX-CFG-MSG. */
#define NMEA_CLASE_ESTANDAR         0xF0
#define NMEA_ID_GGA                 0x00
#define NMEA_ID_GLL                 0x01
#define NMEA_ID_GSA                 0x02
#define NMEA_ID_GSV                 0x03
#define NMEA_ID_RMC                 0x04
#define NMEA_ID_VTG                 0x05

/*
 * Los datos de un fix vienen repartidos en dos tramas: RMC trae posicion,
 * velocidad y validez; GGA trae la altitud. Se guarda lo ultimo de cada
 * una y se vuelca al estado compartido en cada RMC.
 */
typedef struct {
    double latitud;
    double longitud;
    float altitud_m;
    float velocidad_kmh;
    bool con_fix;
} datos_gps_t;

static QueueHandle_t s_cola_uart;

static void ubx_enviar(uint8_t clase, uint8_t id, const uint8_t *carga, uint8_t longitud)
{
    uint8_t trama[8 + UBX_CARGA_MAX];
    if (longitud > UBX_CARGA_MAX) {
        return;
    }

    trama[0] = UBX_SYNC_1;
    trama[1] = UBX_SYNC_2;
    trama[2] = clase;
    trama[3] = id;
    trama[4] = longitud;
    trama[5] = 0;
    memcpy(&trama[6], carga, longitud);

    /* Checksum Fletcher de 8 bits sobre clase, id, longitud y carga. */
    uint8_t ck_a = 0;
    uint8_t ck_b = 0;
    for (int i = 2; i < 6 + longitud; i++) {
        ck_a += trama[i];
        ck_b += ck_a;
    }
    trama[6 + longitud] = ck_a;
    trama[7 + longitud] = ck_b;

    uart_write_bytes(GPS_UART_PUERTO, trama, 8 + longitud);
    uart_wait_tx_done(GPS_UART_PUERTO, pdMS_TO_TICKS(100));
}

static void ubx_fijar_tasa_nmea(uint8_t id_nmea, uint8_t tasa)
{
    const uint8_t carga[3] = { NMEA_CLASE_ESTANDAR, id_nmea, tasa };
    ubx_enviar(UBX_CLASE_CFG, UBX_ID_CFG_MSG, carga, sizeof(carga));
}

/*
 * Configuracion del NEO-M8N al arrancar. No se comprueban los UBX-ACK:
 * si el modulo no responde, simplemente seguira a 9600 baudios y 1 Hz
 * con todas las tramas, y la tarea no recibira nada util hasta el
 * siguiente reinicio. Sin la configuracion guardada en el modulo, esto
 * se repite en cada arranque, que es justo lo que se quiere.
 */
static esp_err_t gps_configurar_modulo(void)
{
    /*
     * UBX-CFG-PRT: UART1 del modulo a GPS_UART_BAUDIOS, 8N1, acepta
     * UBX y NMEA de entrada y emite solo NMEA. Se envia a la velocidad
     * de fabrica. Si el modulo conservaba la configuracion anterior
     * gracias a su pila de respaldo, ya esta a la velocidad alta y
     * esta trama le llega como ruido, pero el resto de la secuencia
     * se envia despues del cambio y le llega bien.
     */
    uint8_t prt[20] = { 0 };
    prt[0] = 1;                                 /* portID: UART1 */
    prt[4] = 0xD0;                              /* mode: 8 bits, sin paridad, 1 stop */
    prt[5] = 0x08;
    prt[8] = (uint8_t)(GPS_UART_BAUDIOS & 0xFF);
    prt[9] = (uint8_t)((GPS_UART_BAUDIOS >> 8) & 0xFF);
    prt[10] = (uint8_t)((GPS_UART_BAUDIOS >> 16) & 0xFF);
    prt[11] = (uint8_t)((GPS_UART_BAUDIOS >> 24) & 0xFF);
    prt[12] = 0x03;                             /* inProtoMask: UBX + NMEA */
    prt[14] = 0x02;                             /* outProtoMask: NMEA */
    ubx_enviar(UBX_CLASE_CFG, UBX_ID_CFG_PRT, prt, sizeof(prt));
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t err = uart_set_baudrate(GPS_UART_PUERTO, GPS_UART_BAUDIOS);
    if (err != ESP_OK) {
        return err;
    }
    uart_flush_input(GPS_UART_PUERTO);

    /* Solo RMC y GGA: el resto no aporta nada aqui y ocupa ancho de banda. */
    ubx_fijar_tasa_nmea(NMEA_ID_GLL, 0);
    ubx_fijar_tasa_nmea(NMEA_ID_GSA, 0);
    ubx_fijar_tasa_nmea(NMEA_ID_GSV, 0);
    ubx_fijar_tasa_nmea(NMEA_ID_VTG, 0);
    ubx_fijar_tasa_nmea(NMEA_ID_RMC, 1);
    ubx_fijar_tasa_nmea(NMEA_ID_GGA, 1);

    /* UBX-CFG-RATE: periodo de medida en ms, una solucion por medida,
     * referencia de tiempo UTC. */
    const uint16_t periodo_ms = 1000 / GPS_FRECUENCIA_HZ;
    const uint8_t rate[6] = {
        (uint8_t)(periodo_ms & 0xFF), (uint8_t)(periodo_ms >> 8),
        1, 0,
        0, 0,
    };
    ubx_enviar(UBX_CLASE_CFG, UBX_ID_CFG_RATE, rate, sizeof(rate));

    /* Da tiempo a que lleguen los ACK binarios y los descarta para que
     * no se cuelen como lineas basura en el parser NMEA. */
    vTaskDelay(pdMS_TO_TICKS(100));
    uart_flush_input(GPS_UART_PUERTO);
    return ESP_OK;
}

static esp_err_t gps_uart_init(void)
{
    const uart_config_t config = {
        .baud_rate = GPS_UART_BAUDIOS_FABRICA,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(GPS_UART_PUERTO, GPS_UART_BUFFER_RX, 0,
                                        GPS_UART_TAM_COLA_EVENTOS, &s_cola_uart, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(GPS_UART_PUERTO, &config);
    if (err != ESP_OK) {
        return err;
    }
    /* PIN_GPS_TX es el TX del ESP32 (va al RX del modulo) y viceversa. */
    err = uart_set_pin(GPS_UART_PUERTO, PIN_GPS_TX, PIN_GPS_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }

    err = gps_configurar_modulo();
    if (err != ESP_OK) {
        return err;
    }

    /* El driver avisa por la cola cada vez que entra un salto de linea,
     * asi la tarea lee trama a trama en vez de trocear el flujo a mano. */
    err = uart_enable_pattern_det_baud_intr(GPS_UART_PUERTO, '\n', 1, 9, 0, 0);
    if (err != ESP_OK) {
        return err;
    }
    uart_pattern_queue_reset(GPS_UART_PUERTO, GPS_UART_TAM_COLA_EVENTOS);
    uart_flush_input(GPS_UART_PUERTO);
    return ESP_OK;
}

/*
 * NMEA codifica las coordenadas como gradosminutos.decimales (ddmm.mmmm).
 * minmea ofrece minmea_tocoord() pero devuelve float, y con 7 cifras
 * significativas se pierden varios metros; aqui se hace lo mismo en double.
 * El signo ya viene aplicado por minmea segun N/S y E/W.
 */
static double coordenada_nmea_a_grados(const struct minmea_float *coord)
{
    if (coord->scale == 0) {
        return NAN;
    }
    double ddmm = (double)coord->value / (double)coord->scale;
    double grados = trunc(ddmm / 100.0);
    double minutos = ddmm - grados * 100.0;
    return grados + minutos / 60.0;
}

static void procesar_rmc(const char *trama, datos_gps_t *datos)
{
    struct minmea_sentence_rmc rmc;
    if (!minmea_parse_rmc(&rmc, trama)) {
        ESP_LOGD(TAG, "RMC no parseable: %s", trama);
        return;
    }

    if (rmc.valid) {
        double lat = coordenada_nmea_a_grados(&rmc.latitude);
        double lon = coordenada_nmea_a_grados(&rmc.longitude);
        float vel = minmea_tofloat(&rmc.speed) * GPS_NUDOS_A_KMH;

        if (!isnan(lat) && !isnan(lon)) {
            datos->latitud = lat;
            datos->longitud = lon;
        }
        datos->velocidad_kmh = isnan(vel) ? 0.0f : vel;

        /* La hora del RMC es UTC. minmea deja -1 si el campo viene vacio. */
        if (rmc.time.hours >= 0) {
            shared_state_write_hora((uint8_t)rmc.time.hours,
                                    (uint8_t)rmc.time.minutes,
                                    (uint8_t)rmc.time.seconds);
        }
    } else {
        datos->velocidad_kmh = 0.0f;
    }

    if (rmc.valid != datos->con_fix) {
        ESP_LOGI(TAG, "fix GPS %s", rmc.valid ? "adquirido" : "perdido");
    }
    datos->con_fix = rmc.valid;

    shared_state_write_gps(datos->latitud, datos->longitud, datos->altitud_m,
                           datos->velocidad_kmh, datos->con_fix);
}

static void procesar_gga(const char *trama, datos_gps_t *datos)
{
    struct minmea_sentence_gga gga;
    if (!minmea_parse_gga(&gga, trama)) {
        ESP_LOGD(TAG, "GGA no parseable: %s", trama);
        return;
    }
    if (gga.fix_quality == 0 || gga.altitude_units != 'M') {
        return;
    }
    float alt = minmea_tofloat(&gga.altitude);
    if (!isnan(alt)) {
        datos->altitud_m = alt;
    }
}

static void procesar_trama(const char *trama, datos_gps_t *datos)
{
    /* strict=true descarta tramas sin checksum o con checksum erroneo. */
    switch (minmea_sentence_id(trama, true)) {
    case MINMEA_SENTENCE_RMC:
        procesar_rmc(trama, datos);
        break;
    case MINMEA_SENTENCE_GGA:
        procesar_gga(trama, datos);
        break;
    case MINMEA_INVALID:
        ESP_LOGD(TAG, "trama invalida o con checksum erroneo: %s", trama);
        break;
    default:
        break;
    }
}

static void descartar_buffer_rx(void)
{
    uart_flush_input(GPS_UART_PUERTO);
    uart_pattern_queue_reset(GPS_UART_PUERTO, GPS_UART_TAM_COLA_EVENTOS);
}

static void leer_trama_pendiente(char *buffer, datos_gps_t *datos)
{
    int pos = uart_pattern_pop_pos(GPS_UART_PUERTO);
    if (pos < 0) {
        /* La cola de posiciones se ha desbordado: se pierde la
         * sincronia con el flujo y toca empezar de cero. */
        ESP_LOGW(TAG, "cola de patrones desbordada, vaciando buffer");
        descartar_buffer_rx();
        return;
    }

    int longitud = pos + 1;
    if (longitud >= GPS_TAM_TRAMA) {
        ESP_LOGW(TAG, "trama de %d bytes demasiado larga, descartada", longitud);
        descartar_buffer_rx();
        return;
    }

    int leidos = uart_read_bytes(GPS_UART_PUERTO, buffer, longitud, pdMS_TO_TICKS(100));
    if (leidos != longitud) {
        ESP_LOGW(TAG, "lectura incompleta de trama (%d de %d bytes)", leidos, longitud);
        descartar_buffer_rx();
        return;
    }

    buffer[leidos] = '\0';
    while (leidos > 0 && (buffer[leidos - 1] == '\n' || buffer[leidos - 1] == '\r')) {
        buffer[--leidos] = '\0';
    }
    if (leidos == 0) {
        return;
    }

    procesar_trama(buffer, datos);
}

static void gps_task_run(void *arg)
{
    static char buffer_trama[GPS_TAM_TRAMA];
    datos_gps_t datos = {
        .latitud = NAN,
        .longitud = NAN,
    };

    esp_err_t err = gps_uart_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo inicializar la UART del GPS: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "tarea GPS arrancada (UART%d, RX=%d, TX=%d, %d baudios, %d Hz)",
             GPS_UART_PUERTO, PIN_GPS_RX, PIN_GPS_TX, GPS_UART_BAUDIOS, GPS_FRECUENCIA_HZ);

    uart_event_t evento;
    while (1) {
        if (xQueueReceive(s_cola_uart, &evento, pdMS_TO_TICKS(GPS_TIMEOUT_SIN_DATOS_MS)) == pdFALSE) {
            /* Sin un solo byte en todo el timeout: el modulo esta
             * desconectado o apagado. No hay que esperar a un RMC con
             * status V para dejar de anunciar un fix que ya no existe. */
            if (datos.con_fix) {
                ESP_LOGW(TAG, "sin datos del GPS, fix perdido");
                datos.con_fix = false;
                datos.velocidad_kmh = 0.0f;
                shared_state_write_gps(datos.latitud, datos.longitud, datos.altitud_m,
                                       0.0f, false);
            }
            continue;
        }

        switch (evento.type) {
        case UART_PATTERN_DET:
            leer_trama_pendiente(buffer_trama, &datos);
            break;
        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "desbordamiento de recepcion (%d), vaciando buffer", evento.type);
            descartar_buffer_rx();
            break;
        default:
            /* UART_DATA y similares no requieren accion: los bytes se
             * quedan en el ring buffer hasta que llega el salto de linea. */
            break;
        }
    }
}

void gps_task_start(void)
{
    xTaskCreatePinnedToCore(gps_task_run, "gps_task", 4096, NULL, 5, NULL, 1);
}
