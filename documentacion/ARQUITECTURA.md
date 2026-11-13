# Arquitectura del sistema propuesto

```text
       ACCION HUMANA (INCLINACION)
                     |
          I2C GPIO6/GPIO7
                     v
       +-------------------------+
       | WAVESHARE ESP32-S3      |
       | QMI8658 ACC X/Y/Z       |
       | LDA entrenado en Python |
       | 5 clases en flash       |
       | LCD GC9A01 (SPI)        |
       | Nodo principal / IA     |
       +-------------------------+
          |                  |
          | ESP-NOW          | Wi-Fi 2.4 GHz
          | 12 bytes         | HTTP POST /api/telemetria
          | comandos/ACK     | HTTP GET /firmware.bin
          v                  v
       +----------------+   +--------------------------+
       | ESP32 DevKit   |   | PC Windows / Python EDGE |
       | Nodo receptor  |   | Dashboard / JSONL        |
       | LED GPIO2      |   | Servidor OTA             |
       +----------------+   +--------------------------+
```

## Relacion con los cuatro requisitos

| Requisito | Implementacion | Archivo que demuestra |
|-----------|----------------|-----------------------|
| OTA | Dos particiones A/B, `esp_https_ota` en LAN, verificacion de arranque y rollback | `nodo_waveshare/main/ota_manager.c`, `partitions.csv`, `edge_server/` |
| IA en ESP32 | LDA supervisado con 915 mediciones QMI8658, pesos embarcados en `modelo_gestos.h` | `nodo_waveshare/main/modelo_ia.c`, `datos_ia/` |
| HMI por gestos | Inclinaciones (4 comandos y reposo), estabilidad temporal, LCD local | `nodo_waveshare/main/main.c`, `pantalla.c` |
| Nodo-nodo y nodo-edge | ESP-NOW con HELLO+ACK; Wi-Fi/HTTP con servidor Python | `red_espnow.c`, `nodo_receptor/main/main.c`, `red_edge.c`, `edge_server/` |

## Arquitectura de software

- El QMI8658 se lee cada 100 ms. La salida textual aparece cada 1500 ms.
- Las mediciones se procesan localmente mediante `score_k=W_k * [AX AY AZ] + b_k`.
- Se elige el mayor score, se aplica softmax y filtro de confianza, y se exigen 4
  resultados consecutivos y retorno al gesto REPOSO.
- La red ESP-NOW descubre la MAC del receptor automáticamente al recibir HELLO.
- El emisor publica comandos de 12 bytes y confirma mediante ACK de aplicación.
- La aplicación mide el tiempo transcurrido hasta ACK (RTT).
- La telemetría JSON se transmite al PC cada 6 s, con el RTT HTTP propio.
- Una inclinación específica sostenida 3.5 s solicita OTA **si hay Wi-Fi**.
- Durante un arranque tras OTA, el firmware se marca válido solo si se inicia
  el sensor IMU; si falla el arranque antes de confirmar, opera el rollback.

## Métricas

1. RTT ESP-NOW: intervalo entre `esp_now_send` y ACK del receptor medido
   en el emisor. No es solo confirmación MAC.
2. RTT HTTP: duración del POST al computador edge, incluye procesamiento y red.
3. Entrega: todavía no existe un registro acumulado de PDR, por lo que no se
   declara una tasa de entrega experimental. Se puede medir contando secuencias
   y ACK en varias repeticiones controladas.

No se comparan MQTT y WebSockets: se entregaron ejemplos, pero su comparación
no figuró en los cuatro requisitos textuales disponibles. Si el ingeniero
exige tres protocolos, se requerirá una práctica complementaria.