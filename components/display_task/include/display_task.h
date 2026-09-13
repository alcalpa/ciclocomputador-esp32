#pragma once

#include "renderer.h"

/* Pantallas definidas en los requisitos de software. */
typedef enum {
    PANTALLA_DATOS = 0,   /* recuadros con todos los datos */
    PANTALLA_MAPA,        /* breadcrumb en V1, tiles offline en V2 */
    PANTALLA_AJUSTES,     /* BLE, brillo, cargar rutas */
    PANTALLA_CANTIDAD,
} pantalla_t;

/* Crea y arranca la tarea de pantalla. Por defecto usa el renderer
 * breadcrumb (V1). */
void display_task_start(void);

/* Cambia la implementacion de renderer usada por la pantalla de mapa.
 * Pensado para poder pasar a renderer_tiles_get() en V2 con un solo
 * cambio de linea en app_main, sin tocar display_task.c. */
void display_task_set_renderer(const renderer_t *renderer);
