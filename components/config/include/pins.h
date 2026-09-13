#pragma once

/*
 * Mapeo de pines del ciclocomputador (ESP32-S3 N16R8).
 * Todo lo que sea "que pin va a que sitio" vive aqui y en ningun otro
 * archivo, para poder cambiar de placa sin tocar la logica.
 *
 * Pines evitados a proposito: 0, 3, 45 y 46 (strapping), 19 y 20 (USB),
 * 26 a 32 (flash SPI) y 33 a 38 (PSRAM octal).
 */

/* ---- Pantalla: SPI2 dedicado ---- */
#define PIN_DISPLAY_SCLK   12
#define PIN_DISPLAY_MOSI   11
#define PIN_DISPLAY_MISO   13
#define PIN_DISPLAY_CS     10
#define PIN_DISPLAY_DC     14
#define PIN_DISPLAY_RST    21

/* ---- Tarjeta SD: SPI3 dedicado, separado del de pantalla ---- */
#define PIN_SD_SCLK        39
#define PIN_SD_MOSI        40
#define PIN_SD_MISO        41
#define PIN_SD_CS          42

/* ---- I2C compartido: BME280 + RTC DS3231 ---- */
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

/* ---- GPS: UART ---- */
#define PIN_GPS_RX         16
#define PIN_GPS_TX         17

/* ---- Sensor de velocidad (iman + Hall) ---- */
#define PIN_SPEED_SENSOR    4   /* con pull-up externo */

/* ---- Bateria ---- */
#define PIN_BATTERY_ADC     1   /* ADC1_CH0 */

/* ---- Botones: encendido, cambiar pantalla, avanzar, seleccionar, atras ---- */
#define PIN_BTN_POWER        5
#define PIN_BTN_NEXT_SCREEN  6
#define PIN_BTN_NEXT_FIELD   7
#define PIN_BTN_SELECT      15
#define PIN_BTN_BACK        18

/* ---- Buzzer ---- */
#define PIN_BUZZER          47
