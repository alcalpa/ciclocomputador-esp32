#include "storage_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pins.h"
#include "shared_state.h"

static const char *TAG = "storage_task";
static bool s_sd_montada = false;

static void storage_task_run(void *arg)
{
    /*
     * TODO:
     * - Montar la SD por HSPI (PIN_SD_*) via esp_vfs_fat_sdspi_mount()
     * - Marcar s_sd_montada = true si el montaje va bien
     * - Leer la ruta GPX seleccionada al arrancar (si existe)
     * - Cada N segundos, escribir en el log GPX la posicion actual
     *   leida de shared_state_read()
     * - En V2: exponer aqui tambien la lectura de tiles de mapa para
     *   que el renderer los pida sin saber nada de SPI/FAT
     */
    ESP_LOGI(TAG, "tarea de almacenamiento arrancada (pendiente montaje SD)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

bool storage_sd_esta_montada(void)
{
    return s_sd_montada;
}

void storage_task_start(void)
{
    xTaskCreatePinnedToCore(storage_task_run, "storage_task", 4096, NULL, 4, NULL, 1);
}
