#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Estado compartido entre tareas.
 *
 * Regla del proyecto: NINGUNA tarea toca los campos de esta struct
 * directamente. Siempre se pasa por shared_state_read() /
 * shared_state_write_*(), que se encargan del mutex por dentro.
 * Asi evitamos condiciones de carrera sin tener que pensar en el
 * mutex en cada modulo.
 */

typedef struct {
    /* GPS */
    double latitud;
    double longitud;
    float altitud_gps_m;
    float velocidad_gps_kmh;
    bool gps_con_fix;

    /* Sensores ambientales (BME280) */
    float temperatura_c;
    float presion_hpa;
    float altitud_barometrica_m;

    /* Velocidad instantanea (sensor de rueda) */
    float velocidad_rueda_kmh;
    float distancia_recorrida_km;

    /* Bateria */
    float bateria_voltaje;
    uint8_t bateria_porcentaje;

    /* BLE */
    bool ble_pulsometro_conectado;
    uint8_t pulsaciones_bpm;
    bool ble_cadencia_conectada;
    uint16_t cadencia_rpm;

    /* Hora (RTC) */
    uint8_t hora;
    uint8_t minuto;
    uint8_t segundo;
} estado_ciclocomputador_t;

/* Inicializa el mutex y pone el estado a valores por defecto. Llamar
 * una sola vez desde app_main antes de crear las tareas. */
void shared_state_init(void);

/* Copia el estado actual en 'destino'. Bloqueante hasta conseguir el mutex. */
void shared_state_read(estado_ciclocomputador_t *destino);

/* Las tareas productoras usan estas funciones para actualizar solo
 * su parte del estado sin pisar lo que escriben las demas. */
void shared_state_write_gps(double lat, double lon, float alt_m, float vel_kmh, bool con_fix);
void shared_state_write_ambiente(float temp_c, float presion_hpa, float alt_baro_m);
void shared_state_write_velocidad_rueda(float vel_kmh, float distancia_km);
void shared_state_write_bateria(float voltaje, uint8_t porcentaje);
void shared_state_write_ble_pulsometro(bool conectado, uint8_t bpm);
void shared_state_write_ble_cadencia(bool conectado, uint16_t rpm);
void shared_state_write_hora(uint8_t h, uint8_t m, uint8_t s);
