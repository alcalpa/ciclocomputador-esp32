#pragma once

#include <stdint.h>

/*
 * Interfaz comun para dibujar la pantalla de mapa/ruta.
 *
 * display_task NUNCA dibuja directamente sobre el framebuffer para
 * esta pantalla: siempre llama a estas dos funciones a traves de un
 * puntero a renderer_t. Esto es lo que permite que V1 (breadcrumb)
 * y V2 (tiles offline desde SD) convivan sin tocar display_task:
 * solo cambia que implementacion se le pasa a display_task_set_renderer().
 */
typedef struct {
    /* Se llama una vez al activar este renderer (reserva de buffers, etc). */
    void (*init)(void);

    /* Se llama en cada refresco de la pantalla de mapa con la posicion
     * actual. El renderer decide como dibujarla (linea acumulada en
     * breadcrumb, o tile + marcador en la version de mapa offline). */
    void (*update)(double latitud, double longitud, float velocidad_kmh);
} renderer_t;

/* Implementacion V1: dibuja la traza recorrida como una linea, sin
 * mapa de fondo. Es la que se usa por defecto al arrancar el proyecto. */
const renderer_t *renderer_breadcrumb_get(void);

/*
 * Implementacion V2 (pendiente): const renderer_t *renderer_tiles_get(void);
 * Leera bitmaps de la SD segun posicion/zoom via storage_task y los
 * pintara como fondo antes de superponer la traza y el marcador.
 * Se anade como archivo nuevo sin modificar renderer.h ni display_task.c.
 */
