# Ciclocomputador ESP32-S3 (ESP-IDF nativo)

Proyecto en construccion sobre el esqueleto inicial de tareas FreeRTOS.
`gps_task` y `sensors_task` ya tienen su logica real; el resto sigue
como esqueleto con TODOs, pendiente de implementarse en el orden
acordado. Compila limpio para `esp32s3` en cada paso.

## Estructura

```
ciclocomputador/
  CMakeLists.txt              raiz del proyecto ESP-IDF
  sdkconfig.defaults          PSRAM, flash, tick de FreeRTOS, BLE (NimBLE)
  main/
    app_main.c                inicializa shared_state y arranca las 5 tareas
  components/
    config/                   pins.h: unico sitio con numeros de pin (pinout real de un
                              ESP32-S3 N16R8, no el de un DevKit generico)
    shared_state/             estado compartido protegido por mutex
    gps_task/                 UART + parser NMEA (minmea), vuelca a shared_state
    minmea/                   parser NMEA 0183 de terceros, copiado como componente local
    sensors_task/             I2C (esp_driver_i2c) con BME280 + DS3231, sensor de
                              rueda por interrupcion GPIO
    ble_task/                 NimBLE: escaneo y sensores externos (pendiente)
    storage_task/             montaje de SD, log GPX, lectura de rutas (pendiente)
    display_task/
      display_task.c          bucle de UI, botones, cambio de pantalla
      renderer/
        renderer.h             interfaz comun del renderer de mapa
        renderer_breadcrumb.c  V1: traza sin mapa de fondo (pendiente de dibujar)
```

## Estado actual

- **`gps_task`** (hecho, sin probar contra hardware real): lee el
  NEO-M8N por UART con deteccion de patron en el driver, y usa
  `minmea` para parsear RMC (posicion, velocidad, validez) y GGA
  (altitud). Al arrancar configura el modulo por UBX: lo sube de 9600
  a 115200 baudios, desactiva las tramas NMEA que no se usan (GLL,
  GSA, GSV, VTG) y fija la tasa a 5 Hz. No se comprueban los ACK de la
  configuracion UBX: si el modulo no responde, se queda en 9600
  baudios y 1 Hz hasta el siguiente reinicio. Tambien vuelca la hora
  UTC del RMC al estado compartido.
- **`sensors_task`** (hecho, sin probar contra hardware real): bus I2C
  con el driver nuevo `esp_driver_i2c` (no el `driver` legacy, que en
  IDF 6.1 ya no incluye I2C). Lee temperatura y presion del BME280 con
  las formulas de compensacion en entero del datasheet, calcula
  altitud barometrica con la formula internacional referida a presion
  estandar a nivel del mar, y lee la hora del DS3231 cuando el GPS no
  tiene fix (si tiene fix, la hora la pone el GPS). El sensor de rueda
  usa una interrupcion GPIO por flanco con antirrebote software; la
  circunferencia de rueda es una constante en codigo hasta que exista
  la calibracion persistida en NVS.
- **Resto de tareas** (`ble_task`, `storage_task`, `display_task`):
  esqueleto con TODOs, sin logica todavia.

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

1. `gps_task` (hecho, ver "Estado actual").
2. `sensors_task` (hecho, ver "Estado actual").
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
