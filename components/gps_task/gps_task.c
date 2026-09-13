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
#define GPS_UART_BAUDIOS            9600    /* velocidad de fabrica del NEO-M8N */
#define GPS_UART_BUFFER_RX          2048
#define GPS_UART_TAM_COLA_EVENTOS   16
#define GPS_TAM_TRAMA               (MINMEA_MAX_SENTENCE_LENGTH + 4)
#define GPS_TIMEOUT_SIN_DATOS_MS    3000
#define GPS_NUDOS_A_KMH             1.852f

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

static esp_err_t gps_uart_init(void)
{
    const uart_config_t config = {
        .baud_rate = GPS_UART_BAUDIOS,
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
    ESP_LOGI(TAG, "tarea GPS arrancada (UART%d, RX=%d, TX=%d, %d baudios)",
             GPS_UART_PUERTO, PIN_GPS_RX, PIN_GPS_TX, GPS_UART_BAUDIOS);

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
