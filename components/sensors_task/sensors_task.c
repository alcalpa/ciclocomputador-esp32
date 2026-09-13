#include "sensors_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pins.h"
#include "shared_state.h"

static const char *TAG = "sensors_task";

static void sensors_task_run(void *arg)
{
    /*
     * TODO:
     * - Inicializar bus I2C en PIN_I2C_SDA / PIN_I2C_SCL
     * - Leer BME280 (temperatura, presion, altitud barometrica)
     * - Leer RTC DS3231 (hora)
     * - Contar pulsos de PIN_SPEED_SENSOR (interrupcion GPIO) para
     *   calcular velocidad instantanea y distancia acumulada segun
     *   la circunferencia de rueda calibrada
     * - Volcar todo con shared_state_write_ambiente() y
     *   shared_state_write_velocidad_rueda()
     */
    ESP_LOGI(TAG, "tarea de sensores arrancada (pendiente BME280/RTC/rueda)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void sensors_task_start(void)
{
    xTaskCreatePinnedToCore(sensors_task_run, "sensors_task", 4096, NULL, 5, NULL, 1);
}
