#include "renderer.h"

#include "esp_log.h"

static const char *TAG = "renderer_breadcrumb";

static void breadcrumb_init(void)
{
    /*
     * TODO:
     * - Reservar el array/buffer donde se acumulan los puntos (lat, lon)
     *   ya convertidos a coordenadas de pantalla
     * - Definir el factor de escala inicial (zoom "encaja todo lo recorrido")
     */
    ESP_LOGI(TAG, "renderer breadcrumb inicializado");
}

static void breadcrumb_update(double latitud, double longitud, float velocidad_kmh)
{
    /*
     * TODO:
     * - Proyectar (latitud, longitud) a coordenadas x,y de pantalla
     *   relativas al primer punto de la ruta
     * - Anadir el punto al buffer de traza
     * - Redibujar: traza acumulada + marcador de posicion actual
     * - Pintar la franja inferior con velocidad_kmh y algun dato mas
     */
    (void)latitud;
    (void)longitud;
    (void)velocidad_kmh;
}

static const renderer_t s_renderer_breadcrumb = {
    .init = breadcrumb_init,
    .update = breadcrumb_update,
};

const renderer_t *renderer_breadcrumb_get(void)
{
    return &s_renderer_breadcrumb;
}
