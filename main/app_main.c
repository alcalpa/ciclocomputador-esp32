#include "esp_log.h"

#include "shared_state.h"
#include "gps_task.h"
#include "sensors_task.h"
#include "ble_task.h"
#include "storage_task.h"
#include "display_task.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "arrancando ciclocomputador");

    /* El estado compartido se inicializa antes que nada: todas las
     * tareas lo usan desde su primera vuelta de bucle. */
    shared_state_init();

    /*
     * Reparto de nucleos:
     * - Core 0: BLE (el stack NimBLE ya usa este nucleo internamente)
     * - Core 1: GPS, sensores, almacenamiento y pantalla
     * Es un punto de partida razonable; se puede reequilibrar mas
     * adelante si algun nucleo se satura.
     */
    ble_task_start();
    gps_task_start();
    sensors_task_start();
    storage_task_start();
    display_task_start();

    ESP_LOGI(TAG, "todas las tareas arrancadas");
}
