#pragma once

/*
 * Mapeo de pines del ciclocomputador.
 * Todo lo que sea "que pin va a que sitio" vive aqui y en ningun otro
 * archivo, para poder cambiar de placa sin tocar la logica.
 */

/* ---- Pantalla: bus VSPI dedicado ---- */
#define PIN_DISPLAY_SCLK   18
#define PIN_DISPLAY_MOSI   23
#define PIN_DISPLAY_MISO   19
#define PIN_DISPLAY_CS      5
#define PIN_DISPLAY_DC      2
#define PIN_DISPLAY_RST     4

/* ---- Tarjeta SD: bus HSPI dedicado, separado del de pantalla ---- */
#define PIN_SD_SCLK        14
#define PIN_SD_MOSI        13
#define PIN_SD_MISO        12
#define PIN_SD_CS          15

/* ---- I2C compartido: BME280 + RTC DS3231 ---- */
#define PIN_I2C_SDA        21
#define PIN_I2C_SCL        22

/* ---- GPS: UART ---- */
#define PIN_GPS_RX         16
#define PIN_GPS_TX         17

/* ---- Sensor de velocidad (iman + Hall) ---- */
#define PIN_SPEED_SENSOR   35   /* input only, con pull-up externo */

/* ---- Bateria ---- */
#define PIN_BATTERY_ADC    34   /* ADC1, input only */

/* ---- Botones: encendido, cambiar pantalla, avanzar, seleccionar, atras ---- */
#define PIN_BTN_POWER      25
#define PIN_BTN_NEXT_SCREEN 26
#define PIN_BTN_NEXT_FIELD 27
#define PIN_BTN_SELECT     32
#define PIN_BTN_BACK       33

/* ---- Buzzer ---- */
#define PIN_BUZZER         38
