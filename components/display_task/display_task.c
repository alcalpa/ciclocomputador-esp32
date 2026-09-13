#include "display_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pins.h"
#include "shared_state.h"

static const char *TAG = "display_task";

static const renderer_t *s_renderer_mapa;
static pantalla_t s_pantalla_actual = PANTALLA_DATOS;

static void inicializar_panel_fisico(void)
{
    /*
     * TODO:
     * - Configurar el bus VSPI con PIN_DISPLAY_* (pins.h)
     * - Inicializar el controlador del panel elegido (memory LCD o
     *   ST7789/ILI9341 segun la pieza final)
     * - Configurar el PWM de brillo si es panel con backlight
     */
    ESP_LOGI(TAG, "panel fisico pendiente de inicializar");
}

static void dibujar_pantalla_datos(const estado_ciclocomputador_t *estado)
{
    /*
     * TODO: dibujar recuadros configurables (2/3/4/6 campos) con los
     * datos de 'estado': velocidad, distancia, altitud, temperatura,
     * pulsaciones, cadencia, hora, bateria...
     */
    (void)estado;
}

static void dibujar_pantalla_mapa(const estado_ciclocomputador_t *estado)
{
    if (s_renderer_mapa != NULL && estado->gps_con_fix) {
        s_renderer_mapa->update(estado->latitud, estado->longitud, estado->velocidad_rueda_kmh);
    }
    /* TODO: pintar franja inferior con 2-3 datos clave debajo del mapa */
}

static void dibujar_pantalla_ajustes(const estado_ciclocomputador_t *estado)
{
    /*
     * TODO: menu de ajustes -> emparejar BLE, brillo, elegir ruta
     * cargada desde SD. Navegacion con los 5 botones (pins.h).
     */
    (void)estado;
}

static void leer_botones(void)
{
    /*
     * TODO:
     * - PIN_BTN_POWER: mantener pulsado -> apagado ordenado
     * - PIN_BTN_NEXT_SCREEN: rota s_pantalla_actual entre las 3 pantallas
     * - PIN_BTN_NEXT_FIELD: dentro de PANTALLA_DATOS, resalta el
     *   siguiente recuadro; dentro de PANTALLA_AJUSTES, mueve el cursor
     * - PIN_BTN_SELECT: confirma la opcion resaltada en ajustes
     * - PIN_BTN_BACK: vuelve atras en el menu de ajustes
     * Mejor como interrupciones GPIO + cola que como polling puro.
     */
}

static void display_task_run(void *arg)
{
    inicializar_panel_fisico();
    s_renderer_mapa = renderer_breadcrumb_get();
    s_renderer_mapa->init();

    estado_ciclocomputador_t estado;

    while (1) {
        leer_botones();
        shared_state_read(&estado);

        switch (s_pantalla_actual) {
            case PANTALLA_DATOS:
                dibujar_pantalla_datos(&estado);
                break;
            case PANTALLA_MAPA:
                dibujar_pantalla_mapa(&estado);
                break;
            case PANTALLA_AJUSTES:
                dibujar_pantalla_ajustes(&estado);
                break;
            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void display_task_set_renderer(const renderer_t *renderer)
{
    s_renderer_mapa = renderer;
    if (s_renderer_mapa != NULL) {
        s_renderer_mapa->init();
    }
}

void display_task_start(void)
{
    xTaskCreatePinnedToCore(display_task_run, "display_task", 8192, NULL, 5, NULL, 1);
}
