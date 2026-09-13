#include "shared_state.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static estado_ciclocomputador_t s_estado;
static SemaphoreHandle_t s_mutex;

void shared_state_init(void)
{
    memset(&s_estado, 0, sizeof(s_estado));
    s_mutex = xSemaphoreCreateMutex();
}

void shared_state_read(estado_ciclocomputador_t *destino)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *destino = s_estado;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_gps(double lat, double lon, float alt_m, float vel_kmh, bool con_fix)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.latitud = lat;
    s_estado.longitud = lon;
    s_estado.altitud_gps_m = alt_m;
    s_estado.velocidad_gps_kmh = vel_kmh;
    s_estado.gps_con_fix = con_fix;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_ambiente(float temp_c, float presion_hpa, float alt_baro_m)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.temperatura_c = temp_c;
    s_estado.presion_hpa = presion_hpa;
    s_estado.altitud_barometrica_m = alt_baro_m;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_velocidad_rueda(float vel_kmh, float distancia_km)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.velocidad_rueda_kmh = vel_kmh;
    s_estado.distancia_recorrida_km = distancia_km;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_bateria(float voltaje, uint8_t porcentaje)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.bateria_voltaje = voltaje;
    s_estado.bateria_porcentaje = porcentaje;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_ble_pulsometro(bool conectado, uint8_t bpm)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.ble_pulsometro_conectado = conectado;
    s_estado.pulsaciones_bpm = bpm;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_ble_cadencia(bool conectado, uint16_t rpm)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.ble_cadencia_conectada = conectado;
    s_estado.cadencia_rpm = rpm;
    xSemaphoreGive(s_mutex);
}

void shared_state_write_hora(uint8_t h, uint8_t m, uint8_t s)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_estado.hora = h;
    s_estado.minuto = m;
    s_estado.segundo = s;
    xSemaphoreGive(s_mutex);
}
