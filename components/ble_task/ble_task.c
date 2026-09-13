#include "ble_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "shared_state.h"

static const char *TAG = "ble_task";

static void ble_task_run(void *arg)
{
    /*
     * TODO:
     * - Inicializar el stack NimBLE
     * - Escanear dispositivos con perfil Cycling Speed and Cadence
     *   o Heart Rate (GATT estandar)
     * - Guardar en NVS las direcciones de los sensores ya emparejados
     *   para reconectar solo al arrancar
     * - Por cada notificacion GATT recibida, llamar a
     *   shared_state_write_ble_pulsometro() / shared_state_write_ble_cadencia()
     */
    ESP_LOGI(TAG, "tarea BLE arrancada (pendiente stack NimBLE)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ble_task_start(void)
{
    xTaskCreatePinnedToCore(ble_task_run, "ble_task", 4096, NULL, 5, NULL, 0);
}
