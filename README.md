# Ciclocomputador ESP32-S3 (ESP-IDF nativo)

Esqueleto inicial del proyecto. Compila (con TODOs) pero ningun modulo
tiene aun la logica real: sirve como punto de partida sobre el que ir
implementando cada tarea por separado, en el orden acordado.

## Estructura

```
ciclocomputador/
  CMakeLists.txt              raiz del proyecto ESP-IDF
  sdkconfig.defaults          PSRAM, flash, tick de FreeRTOS, BLE (NimBLE)
  main/
    app_main.c                inicializa shared_state y arranca las 5 tareas
  components/
    config/                   pins.h: unico sitio con numeros de pin
    shared_state/             estado compartido protegido por mutex
    gps_task/                 UART + parser NMEA (minmea), vuelca a shared_state
    minmea/                   parser NMEA 0183 de terceros, copiado como componente local
    sensors_task/             I2C (BME280 + RTC) + sensor de rueda (pendiente)
    ble_task/                 NimBLE: escaneo y sensores externos (pendiente)
    storage_task/             montaje de SD, log GPX, lectura de rutas (pendiente)
    display_task/
      display_task.c          bucle de UI, botones, cambio de pantalla
      renderer/
        renderer.h             interfaz comun del renderer de mapa
        renderer_breadcrumb.c  V1: traza sin mapa de fondo (pendiente de dibujar)
```

## Por que esta forma

- **`config/pins.h` centralizado**: si cambia el cableado, se toca un
  solo archivo.
- **`shared_state` como unico punto de paso de datos**: ninguna tarea
  conoce a las demas directamente, todas leen/escriben el estado
  compartido. Esto hace que anadir o quitar una tarea no afecte al
  resto.
- **`renderer.h` como interfaz**: `display_task.c` no sabe si esta
  dibujando un breadcrumb o un mapa con tiles. Anadir V2 sera crear
  `renderer_tiles.c` que implemente la misma interfaz y cambiar una
  linea en `display_task_run()` (o exponerlo como opcion de ajustes).
- **Reparto de nucleos**: BLE en el core 0 (donde ya corre parte del
  stack internamente), el resto en el core 1. Es un punto de partida,
  no una decision definitiva.

## Compilar (cuando haya un ESP-IDF instalado)

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Orden de implementacion sugerido

1. `gps_task`: UART + parser NMEA con `minmea` (componente local en
   `components/minmea/`), volcando a `shared_state_write_gps()`. Hecho,
   pendiente de probar contra el modulo real.
2. `sensors_task`: I2C con BME280, y despues el sensor de rueda por
   interrupcion GPIO.
3. `display_task`: primero `PANTALLA_DATOS` con datos reales del
   estado compartido (sin BLE ni mapa todavia).
4. `storage_task`: montaje de SD y escritura de log GPX basico.
5. `ble_task`: NimBLE, empezando por un unico perfil (p.ej. Heart Rate)
   antes de anadir cadencia/potencia.
6. `PANTALLA_MAPA` con `renderer_breadcrumb`.
7. `PANTALLA_AJUSTES` (brillo, emparejamiento BLE, seleccion de ruta).
8. V2: `renderer_tiles.c` + script externo de preparacion de tiles.

Cada punto de la lista de checklist (`ciclocomputador-esp32-checklist.md`)
deberia poder marcarse a medida que se completa el modulo
correspondiente de esta lista.
