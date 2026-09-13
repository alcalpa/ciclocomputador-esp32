#include "sensors_task.h"

#include <math.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "pins.h"
#include "shared_state.h"

static const char *TAG = "sensors_task";

#define SENSORS_PERIODO_MS          500

#define I2C_FRECUENCIA_HZ           400000
#define I2C_TIMEOUT_MS              100

/* BME280: 0x76 con SDO a GND (lo habitual en los modulos), 0x77 con SDO a VCC. */
#define BME280_DIRECCION            0x76
#define BME280_DIRECCION_ALT        0x77
#define BME280_ID_ESPERADO          0x60
#define BME280_REG_ID               0xD0
#define BME280_REG_RESET            0xE0
#define BME280_REG_CALIBRACION      0x88
#define BME280_REG_CTRL_HUM         0xF2
#define BME280_REG_CTRL_MEAS        0xF4
#define BME280_REG_CONFIG           0xF5
#define BME280_REG_DATOS            0xF7
#define BME280_CMD_RESET            0xB6
/* Temperatura x2, presion x16, modo normal. Sin humedad: el estado
 * compartido no la usa. */
#define BME280_CTRL_MEAS_VALOR      0x57
/* Standby 250 ms y filtro IIR x4: suaviza la altitud sin retrasarla demasiado. */
#define BME280_CONFIG_VALOR         0x68

#define DS3231_DIRECCION            0x68
#define DS3231_REG_SEGUNDOS         0x00
#define DS3231_REG_ESTADO           0x0F
#define DS3231_BIT_OSF              0x80
#define DS3231_BIT_MODO_12H         0x40
#define DS3231_BIT_PM               0x20

#define PRESION_NIVEL_MAR_HPA       1013.25f

/* Circunferencia de una 700x25c. Pendiente de calibracion y persistencia en NVS. */
#define RUEDA_CIRCUNFERENCIA_M      2.105f
#define RUEDA_ANTIRREBOTE_US        20000
#define RUEDA_TIMEOUT_PARADO_US     3000000

typedef struct {
    uint16_t t1;
    int16_t t2;
    int16_t t3;
    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;
} bme280_calibracion_t;

static i2c_master_bus_handle_t s_bus_i2c;
static i2c_master_dev_handle_t s_bme280;
static i2c_master_dev_handle_t s_ds3231;
static bme280_calibracion_t s_calib;

/* Compartido entre la ISR del sensor de rueda y la tarea. */
static portMUX_TYPE s_rueda_spinlock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_rueda_pulsos;
static volatile int64_t s_rueda_ultimo_pulso_us;
static volatile int64_t s_rueda_intervalo_us;

/* ---- I2C ---- */

static esp_err_t i2c_leer_registros(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *datos, size_t longitud)
{
    return i2c_master_transmit_receive(dev, &reg, 1, datos, longitud, I2C_TIMEOUT_MS);
}

static esp_err_t i2c_escribir_registro(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t valor)
{
    const uint8_t trama[2] = { reg, valor };
    return i2c_master_transmit(dev, trama, sizeof(trama), I2C_TIMEOUT_MS);
}

static esp_err_t i2c_anadir_dispositivo(uint16_t direccion, i2c_master_dev_handle_t *dev)
{
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = direccion,
        .scl_speed_hz = I2C_FRECUENCIA_HZ,
    };
    return i2c_master_bus_add_device(s_bus_i2c, &config, dev);
}

static esp_err_t i2c_init(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = -1,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&config, &s_bus_i2c);
}

/* ---- BME280 ---- */

static esp_err_t bme280_init(void)
{
    uint16_t direccion = BME280_DIRECCION;
    if (i2c_master_probe(s_bus_i2c, direccion, I2C_TIMEOUT_MS) != ESP_OK) {
        direccion = BME280_DIRECCION_ALT;
        if (i2c_master_probe(s_bus_i2c, direccion, I2C_TIMEOUT_MS) != ESP_OK) {
            return ESP_ERR_NOT_FOUND;
        }
    }

    esp_err_t err = i2c_anadir_dispositivo(direccion, &s_bme280);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t id = 0;
    err = i2c_leer_registros(s_bme280, BME280_REG_ID, &id, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (id != BME280_ID_ESPERADO) {
        ESP_LOGE(TAG, "id 0x%02X en 0x%02X, no es un BME280", id, direccion);
        return ESP_ERR_NOT_SUPPORTED;
    }

    err = i2c_escribir_registro(s_bme280, BME280_REG_RESET, BME280_CMD_RESET);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Los coeficientes de calibracion van en little endian, T1 y P1 sin signo. */
    uint8_t c[24];
    err = i2c_leer_registros(s_bme280, BME280_REG_CALIBRACION, c, sizeof(c));
    if (err != ESP_OK) {
        return err;
    }
    s_calib.t1 = (uint16_t)(c[0] | (c[1] << 8));
    s_calib.t2 = (int16_t)(c[2] | (c[3] << 8));
    s_calib.t3 = (int16_t)(c[4] | (c[5] << 8));
    s_calib.p1 = (uint16_t)(c[6] | (c[7] << 8));
    s_calib.p2 = (int16_t)(c[8] | (c[9] << 8));
    s_calib.p3 = (int16_t)(c[10] | (c[11] << 8));
    s_calib.p4 = (int16_t)(c[12] | (c[13] << 8));
    s_calib.p5 = (int16_t)(c[14] | (c[15] << 8));
    s_calib.p6 = (int16_t)(c[16] | (c[17] << 8));
    s_calib.p7 = (int16_t)(c[18] | (c[19] << 8));
    s_calib.p8 = (int16_t)(c[20] | (c[21] << 8));
    s_calib.p9 = (int16_t)(c[22] | (c[23] << 8));

    /* ctrl_hum se escribe antes que ctrl_meas aunque no se use, porque
     * el sensor solo aplica ctrl_hum al escribir ctrl_meas. */
    err = i2c_escribir_registro(s_bme280, BME280_REG_CTRL_HUM, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_escribir_registro(s_bme280, BME280_REG_CONFIG, BME280_CONFIG_VALOR);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_escribir_registro(s_bme280, BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_VALOR);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "BME280 detectado en 0x%02X", direccion);
    return ESP_OK;
}

/*
 * Formulas de compensacion en entero del datasheet de Bosch (seccion 4.2.3),
 * con los nombres originales de las variables para poder cotejarlas.
 * Devuelven temperatura en centesimas de grado y presion en Pa con 8 bits
 * de fraccion.
 */
static int32_t bme280_compensar_temperatura(int32_t adc_t, int32_t *t_fine)
{
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)s_calib.t1 << 1))) * ((int32_t)s_calib.t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)s_calib.t1)) * ((adc_t >> 4) - ((int32_t)s_calib.t1))) >> 12)
                    * ((int32_t)s_calib.t3)) >> 14;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;
}

static uint32_t bme280_compensar_presion(int32_t adc_p, int32_t t_fine)
{
    int64_t var1 = ((int64_t)t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)s_calib.p6;
    var2 = var2 + ((var1 * (int64_t)s_calib.p5) << 17);
    var2 = var2 + (((int64_t)s_calib.p4) << 35);
    var1 = ((var1 * var1 * (int64_t)s_calib.p3) >> 8) + ((var1 * (int64_t)s_calib.p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)s_calib.p1) >> 33;
    if (var1 == 0) {
        return 0;
    }
    int64_t p = 1048576 - adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)s_calib.p9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)s_calib.p8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)s_calib.p7) << 4);
    return (uint32_t)p;
}

static esp_err_t bme280_leer(float *temperatura_c, float *presion_hpa)
{
    uint8_t d[6];
    esp_err_t err = i2c_leer_registros(s_bme280, BME280_REG_DATOS, d, sizeof(d));
    if (err != ESP_OK) {
        return err;
    }

    int32_t adc_p = (int32_t)(((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | ((uint32_t)d[2] >> 4));
    int32_t adc_t = (int32_t)(((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | ((uint32_t)d[5] >> 4));

    int32_t t_fine;
    int32_t temp_centesimas = bme280_compensar_temperatura(adc_t, &t_fine);
    uint32_t presion_q24_8 = bme280_compensar_presion(adc_p, t_fine);
    if (presion_q24_8 == 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    *temperatura_c = temp_centesimas / 100.0f;
    *presion_hpa = presion_q24_8 / 256.0f / 100.0f;
    return ESP_OK;
}

/* Formula barometrica internacional referida a la presion estandar a
 * nivel del mar. La altitud absoluta variara con el tiempo atmosferico;
 * lo que importa para el desnivel acumulado es la variacion relativa. */
static float altitud_barometrica(float presion_hpa)
{
    return 44330.0f * (1.0f - powf(presion_hpa / PRESION_NIVEL_MAR_HPA, 0.1903f));
}

/* ---- DS3231 ---- */

static esp_err_t ds3231_init(void)
{
    if (i2c_master_probe(s_bus_i2c, DS3231_DIRECCION, I2C_TIMEOUT_MS) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    return i2c_anadir_dispositivo(DS3231_DIRECCION, &s_ds3231);
}

static uint8_t bcd_a_binario(uint8_t bcd)
{
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

/* Devuelve ESP_ERR_INVALID_STATE si el RTC perdio la alimentacion y su
 * hora no es fiable (bit OSF del registro de estado). */
static esp_err_t ds3231_leer_hora(uint8_t *hora, uint8_t *minuto, uint8_t *segundo)
{
    uint8_t estado;
    esp_err_t err = i2c_leer_registros(s_ds3231, DS3231_REG_ESTADO, &estado, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (estado & DS3231_BIT_OSF) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t r[3];
    err = i2c_leer_registros(s_ds3231, DS3231_REG_SEGUNDOS, r, sizeof(r));
    if (err != ESP_OK) {
        return err;
    }

    *segundo = bcd_a_binario(r[0] & 0x7F);
    *minuto = bcd_a_binario(r[1] & 0x7F);
    if (r[2] & DS3231_BIT_MODO_12H) {
        uint8_t h = bcd_a_binario(r[2] & 0x1F) % 12;
        *hora = (r[2] & DS3231_BIT_PM) ? h + 12 : h;
    } else {
        *hora = bcd_a_binario(r[2] & 0x3F);
    }
    return ESP_OK;
}

/* ---- Sensor de rueda ---- */

static void IRAM_ATTR rueda_isr(void *arg)
{
    int64_t ahora = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&s_rueda_spinlock);
    if (s_rueda_ultimo_pulso_us == 0) {
        /* Primer pulso: cuenta pero no hay intervalo con el que medir. */
        s_rueda_pulsos++;
        s_rueda_ultimo_pulso_us = ahora;
    } else {
        int64_t intervalo = ahora - s_rueda_ultimo_pulso_us;
        if (intervalo >= RUEDA_ANTIRREBOTE_US) {
            s_rueda_pulsos++;
            s_rueda_intervalo_us = intervalo;
            s_rueda_ultimo_pulso_us = ahora;
        }
    }
    portEXIT_CRITICAL_ISR(&s_rueda_spinlock);
}

static esp_err_t rueda_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << PIN_SPEED_SENSOR,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    /* El servicio de ISR es global: otra tarea (botones) puede haberlo
     * instalado ya, y eso no es un error. */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    return gpio_isr_handler_add(PIN_SPEED_SENSOR, rueda_isr, NULL);
}

static void rueda_calcular(float *velocidad_kmh, float *distancia_km)
{
    portENTER_CRITICAL(&s_rueda_spinlock);
    uint32_t pulsos = s_rueda_pulsos;
    int64_t ultimo_us = s_rueda_ultimo_pulso_us;
    int64_t intervalo_us = s_rueda_intervalo_us;
    portEXIT_CRITICAL(&s_rueda_spinlock);

    *distancia_km = pulsos * RUEDA_CIRCUNFERENCIA_M / 1000.0f;

    int64_t desde_ultimo_us = esp_timer_get_time() - ultimo_us;
    if (intervalo_us <= 0 || desde_ultimo_us > RUEDA_TIMEOUT_PARADO_US) {
        *velocidad_kmh = 0.0f;
        return;
    }
    /* Velocidad instantanea a partir de la ultima vuelta completa. */
    *velocidad_kmh = RUEDA_CIRCUNFERENCIA_M / (intervalo_us / 1000000.0f) * 3.6f;
}

/* ---- Tarea ---- */

static void sensors_task_run(void *arg)
{
    bool bme280_ok = false;
    bool ds3231_ok = false;
    bool rueda_ok = false;

    esp_err_t err = i2c_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo crear el bus I2C: %s", esp_err_to_name(err));
    } else {
        err = bme280_init();
        bme280_ok = (err == ESP_OK);
        if (!bme280_ok) {
            ESP_LOGE(TAG, "BME280 no disponible: %s", esp_err_to_name(err));
        }

        err = ds3231_init();
        ds3231_ok = (err == ESP_OK);
        if (!ds3231_ok) {
            ESP_LOGE(TAG, "DS3231 no disponible: %s", esp_err_to_name(err));
        }
    }

    err = rueda_init();
    rueda_ok = (err == ESP_OK);
    if (!rueda_ok) {
        ESP_LOGE(TAG, "sensor de rueda no disponible: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "tarea de sensores arrancada (BME280 %s, DS3231 %s, rueda %s)",
             bme280_ok ? "ok" : "no", ds3231_ok ? "ok" : "no", rueda_ok ? "ok" : "no");

    bool rtc_sin_hora_avisado = false;
    TickType_t ultimo_despertar = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&ultimo_despertar, pdMS_TO_TICKS(SENSORS_PERIODO_MS));

        if (bme280_ok) {
            float temperatura_c;
            float presion_hpa;
            if (bme280_leer(&temperatura_c, &presion_hpa) == ESP_OK) {
                shared_state_write_ambiente(temperatura_c, presion_hpa,
                                            altitud_barometrica(presion_hpa));
            } else {
                ESP_LOGW(TAG, "fallo leyendo el BME280");
            }
        }

        if (ds3231_ok) {
            /* Mientras el GPS tiene fix la hora la pone el, que es la
             * referencia buena. El RTC solo cubre el hueco sin fix. */
            estado_ciclocomputador_t estado;
            shared_state_read(&estado);
            if (!estado.gps_con_fix) {
                uint8_t h, m, s;
                err = ds3231_leer_hora(&h, &m, &s);
                if (err == ESP_OK) {
                    shared_state_write_hora(h, m, s);
                } else if (err == ESP_ERR_INVALID_STATE && !rtc_sin_hora_avisado) {
                    ESP_LOGW(TAG, "el DS3231 perdio la alimentacion, hora no fiable hasta sincronizar");
                    rtc_sin_hora_avisado = true;
                }
            }
        }

        if (rueda_ok) {
            float velocidad_kmh;
            float distancia_km;
            rueda_calcular(&velocidad_kmh, &distancia_km);
            shared_state_write_velocidad_rueda(velocidad_kmh, distancia_km);
        }
    }
}

void sensors_task_start(void)
{
    xTaskCreatePinnedToCore(sensors_task_run, "sensors_task", 4096, NULL, 5, NULL, 1);
}
