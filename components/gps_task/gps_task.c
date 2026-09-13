#include "gps_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pins.h"
#include "shared_state.h"

static const char *TAG = "gps_task";

static void gps_task_run(void *arg)
{
    /*
     * TODO:
     * - Inicializar UART en PIN_GPS_RX / PIN_GPS_TX
     * - Leer tramas NMEA y parsearlas (libreria minmea o parser propio)
     * - Por cada fix valido, llamar a shared_state_write_gps(...)
     */
    ESP_LOGI(TAG, "tarea GPS arrancada (pendiente de implementar parser NMEA)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void gps_task_start(void)
{
    xTaskCreatePinnedToCore(gps_task_run, "gps_task", 4096, NULL, 5, NULL, 1);
}
