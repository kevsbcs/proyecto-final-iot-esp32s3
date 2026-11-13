# Revisión del material proporcionado por el ingeniero

**Analizados** los archivos adjuntos recibidos en la conversación, incluyendo
`esp_now_transmisor.zip`, `esp_now_receptor.zip`, `main(1).c`, `main.c`,
`idf_component(1).yml`, `idf_component.yml`, `CMakeLists(1).txt`,
`CMakeLists.txt`, `partitions.csv`, `tinyml_01.ipynb`, `randomforest.c`,
`kmeans.c`, `rtc_ds3231_i2c.txt`, `dataloger.txt`, `memoria.txt`,
`componentes esp32.docx` y el ejemplo SDSPI/FATFS.

## Lo que sí aporta cada archivo

| Archivo/material | Función formativa | Decisión para este proyecto |
|---|---|---|
| `esp_now_transmisor.zip` (main/1..8 + main.c) | Comunicación peer, broadcast, send_cb, Wi-Fi STA, PMK/LMK | Adaptar a API v6.1 y paquete fijo de 12 bytes |
| `esp_now_receptor.zip` | Receptor callback, LED y respuestas de estado | Receptor con validación de longitud, cola y ACK |
| `main(1).c` | QMI8658, GC9A01, SPI, LVGL, pitch/roll | Aprovechar pinout Touch y técnica; sustituir LVGL por LCD liviana |
| `idf_component(1).yml` | GC9A01, CST816S, LVGL8 como dependencias | Instalar solo GC9A01 v2 vía registry |
| `main.c` (SMART_GATEWAY) | MQTT, FreeRTOS, colas, SD, LCD, sensores SHT31/PCF8563 | Arquitectura conceptual; sensores y pines son de otra variante |
| `partitions.csv` | Tabla factory para app grande | Cambiar a `ota_0`, `ota_1`, `otadata` |
| `tinyml_01.ipynb` | Ejemplo de entrenamiento y exportación .tflite de seno | Referencia didáctica; modelo real del equipo es LDA con CSV propio |
| `randomforest.c` | Inferencia por z-score + umbral (aunque archivo se llama RF) | Referencia de anomalía, no Random Forest real |
| `kmeans.c` | Asignación de clústeres a centroides predefinidos | Referencia no supervisada; no apto sin adaptar datos |
| `rtc_ds3231_i2c.txt` | Lectura RTC externo con I2C | No aplicable sin reloj DS3231 instalado |
| `dataloger.txt` | Productor/consumidor, queue, CSV en SD | Técnica reutilizable; hardware ajeno |
| `memoria.txt` y ejemplo FATFS | Uso de SD por SPI, FAT | Funcionalidad optativa, no exigida en requisitos centrales |
| `componentes esp32.docx` | Administrador de componentes IDF 5.5.2 | Adaptar a IDF 6.1 y solo dependencias realmente usadas |

## Problemas concretos detectados en códigos del profesor

1. **Longitud de payload:** varios ejemplos declaran `uint8_t datos[2]` o
   cadenas cortas, pero ejecutan `esp_now_send(..., datos, 32)` sin disponer
   de 32 bytes: lectura fuera de límites. Nuestro protocolo usa
   `sizeof(iot_trama_t)` y 12 bytes exactos.
2. **Callbacks de versión:** `send_cb(const uint8_t *mac_addr, ...)` no
   coincide con la firma de ESP-IDF 6.1; se cambió a
   `const esp_now_send_info_t *`.
3. **MACs hardcodeadas:** los arrays `peer_mac` de clase son direcciones
   pertenecientes a las placas de ejemplo. Nuestra implementación descubre
   la MAC del receptor a partir de tramas HELLO.
4. **Canales:** ESP-NOW y Wi-Fi STA asociada deben compartir canal en un
   mismo transceptor; conectar la Waveshare a un AP cambia el canal de radio.
5. **Procesamiento de callbacks:** las rutinas de recibir deben ser cortas;
   se usa cola FreeRTOS para manejar LED/comandos fuera de la tarea Wi-Fi.
6. **I2C timeout:** parte de `main(1).c` pasa `pdMS_TO_TICKS(100)` como
   timeout a APIs `i2c_master_transmit`; esas APIs reciben tiempo en
   **milisegundos**, no ticks.
7. **Conflicto de placas:** `main(1).c` usa BL=2 y RST=14 propios del modelo
   Waveshare Touch. `main.c` SMART_GATEWAY usa BL=40 y RST=12,
   correspondencia de otro modelo Waveshare sin táctil.
8. **Sensores:** SHT31 y RTC PCF8563/DS3231 se suponen en el gateway y no
   aparecieron en el escaneo I2C real (0x6B y 0x15). No fabricar lecturas
   de temperatura ni declararlas componentes físicos de la Waveshare.
9. **OTA:** la tabla adjunta solo tiene `factory` y no `otadata` ni dos
   apps OTA. Nueva tabla para 16 MB incluida en el proyecto.
10. **IA de ejemplo:** `randomforest.c` no implementa un bosque aleatorio;
    calcula distancia euclídea normalizada contra valores de referencia.
    El código `kmeans.c` usa centroides codificados, no entrenamiento en ESP.
    Nuestro LDA sí procede de entrenamiento supervisado con datos reales.

## Fuentes verificadas

- [Waveshare ESP32-S3 Touch LCD 1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)
- [Espressif ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html)
- [Ejemplo ESP-NOW de Espressif](https://github.com/espressif/esp-idf/tree/master/examples/wifi/espnow)
- [Componente Espressif GC9A01](https://components.espressif.com/components/espressif/esp_lcd_gc9a01)
- [OTA en ESP-IDF](https://github.com/espressif/esp-idf/tree/master/examples/system/ota)