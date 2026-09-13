#pragma once

#include <stdbool.h>

/* Crea y arranca la tarea de almacenamiento (montaje de SD, lectura
 * de rutas GPX y escritura periodica del log de la ruta actual). */
void storage_task_start(void);

/* Disponible para otros modulos (p.ej. el renderer de tiles en V2)
 * que necesiten saber si la SD esta montada antes de leer de ella. */
bool storage_sd_esta_montada(void);
