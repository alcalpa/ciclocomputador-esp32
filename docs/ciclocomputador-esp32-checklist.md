# Ciclocomputador ESP32 — Checklist del proyecto

Documento vivo para ir marcando lo que se ha ido resolviendo. Organizado en piezas (V1 + anexo V2), requisitos físicos y requisitos de software.

---

## 1. Piezas — V1 (breadcrumb)

Compradas pensando ya en no tener que rehacer nada para V2.

- [ ] MCU: ESP32-S3, variante N16R8 (16MB flash / 8MB PSRAM), formato compacto tipo "supermini" (no el DevKit de desarrollo)
- [ ] Pantalla: Memory LCD tipo Sharp/JDI (recomendada, bajo consumo + visible al sol) **o** IPS ST7789/ILI9341 con brillo controlado por PWM
- [ ] GPS: NEO-M8N + antena activa
- [ ] Sensor ambiental: BME280 (temperatura, presión/altitud, humedad) — bus I2C
- [ ] Sensor de velocidad: imán de rueda + sensor Hall (o reed switch)
- [ ] RTC: DS3231 — bus I2C
- [ ] Almacenamiento: módulo SD (clase 10 / UHS-1) + tarjeta 8-32GB
- [ ] Batería: LiPo 3000-5000 mAh
- [ ] Carga/protección: módulo TP4056 con protección + puerto USB-C
- [ ] Botones: 5x pulsador táctil momentáneo (encendido, cambiar pantalla, avanzar dato, seleccionar, retroceder)
- [ ] Bluetooth: BLE nativo del ESP32-S3 (sin componente adicional)
- [ ] Buzzer pasivo
- [ ] Interruptor físico de corte real de batería (microswitch deslizante o pulsador con auto-latch)
- [ ] Conectores JST-PH con lengüeta de bloqueo
- [ ] Carcasa impresa en 3D con junta/silicona en aberturas

### Buses y pines (definidos)

- [x] SPI2 dedicado a pantalla: SCLK=12, MOSI=11, MISO=13, CS=10, DC=14, RST=21
- [x] SPI3 dedicado a SD: SCLK=39, MOSI=40, MISO=41, CS=42
- [x] I2C compartido para BME280 + RTC: SDA=8, SCL=9
- [x] UART para GPS: RX=16, TX=17
- [x] Sensor de rueda: GPIO 4
- [x] ADC para lectura de batería: GPIO 1 (ADC1_CH0)
- [x] Botones: 5, 6, 7, 15, 18 (con pull-up interno)
- [x] Buzzer: GPIO 47
- Evitados: 0, 3, 45, 46 (strapping), 19, 20 (USB), 26-32 (flash), 33-38 (PSRAM octal)

---

## 2. Anexo — Cambios para V2 (mapa offline con tiles)

No implica tocar hardware si se compró según la lista de V1.

- [ ] Implementar el `Renderer` de tiles (lectura de bitmaps desde SD según posición GPS y nivel de zoom)
- [ ] Script externo (PC) para descargar tiles de OpenStreetMap de la zona de la ruta y convertirlos a formato ligero (1-bit si es memory LCD, RGB565 si es TFT color)
- [ ] Verificar capacidad de la SD para varios niveles de zoom de la ruta completa
- [ ] Si se usa memory LCD monocroma: asumir que el mapa se verá como líneas/contornos, no como mapa a color

---

## 3. Requisitos físicos

| # | Requisito | Notas |
|---|---|---|
| 1 | Compacto | Módulo ESP32-S3 pequeño, no DevKit completo |
| 2 | Pantalla de bajo consumo o visible al sol | Memory LCD recomendada |
| 3 | 5 botones | Encendido, cambiar pantalla, avanzar dato, seleccionar, retroceder |
| 4 | Bluetooth para dispositivos externos | BLE nativo |
| 5 | Tarjeta SD para mapas y rutas | Bus SPI dedicado |
| 6 | GPS, altímetro, temperatura, velocímetro | GPS + BME280 + sensor de rueda |

Añadidos identificados durante el diseño:

- [ ] Estanqueidad: juntas o silicona en carcasa, botones y puerto de carga
- [ ] Sistema de anclaje al manillar (cuarto de vuelta tipo Garmin/Wahoo, o GoPro)
- [ ] Posición de la antena GPS: orientada al cielo, alejada de pantalla/batería
- [ ] Aislamiento de vibración: conectores con lengüeta, tornillos con arandela de goma

---

## 4. Requisitos de software

| # | Requisito | Notas |
|---|---|---|
| 1 | Pantalla principal con datos en recuadros | Grid configurable (2/3/4/6 campos) |
| 2 | Pantalla de mapa (breadcrumb → tiles) + datos abajo | Franja inferior con 2-3 datos clave |
| 3 | Pantalla de ajustes: BLE, brillo, cargar rutas | — |
| 4 | Lectura/escritura de SD | Lectura de rutas GPX planificadas + escritura de logs |

Añadidos identificados durante el diseño:

- [ ] Gestión de energía: auto-apagado de pantalla, detección de "parado", indicador de batería con curva real de descarga
- [ ] Flujo de navegación entre pantallas definido como diagrama de estados antes de programar el menú
- [ ] Persistencia de ajustes en NVS: brillo, unidades, última ruta cargada, circunferencia de rueda
- [ ] Calibración del sensor de rueda (circunferencia)
- [ ] Registro GPX de la ruta (formato estándar para Strava/Komoot)
- [ ] Gestión de emparejamiento BLE (reconexión automática a sensores ya conocidos)

### Nice-to-have (no bloquean el MVP)

- [ ] Alertas de "fuera de ruta"
- [ ] Actualización de firmware por WiFi (OTA)
- [ ] Detección automática de parada larga (pausa de cronómetro)
- [ ] Perfiles de usuario/bici (varias ruedas/bicis)
- [ ] Nota: ANT+ no es viable con este hardware; asumir solo sensores BLE

---

## 5. Arquitectura de software (referencia)

- [ ] Tareas FreeRTOS independientes: `gps_task`, `sensors_task`, `ble_task`, `display_task`, `storage_task`
- [ ] Estado compartido protegido por mutex/cola entre tareas
- [ ] `display_task` nunca dibuja directamente: siempre delega en un `Renderer` con interfaz común (`update(posición, ruta)`)
- [ ] Implementación `Renderer` V1: breadcrumb
- [ ] Implementación `Renderer` V2: tiles offline desde SD (se añade sin tocar el resto del sistema)
