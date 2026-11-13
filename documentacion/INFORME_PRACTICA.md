# Implementación de un nodo IoT inteligente con ESP32-S3

**Asignatura:** IoT — Ingeniería Electrónica  
**Fecha:** 13 de noviembre de 2026  
**Plataforma:** Waveshare ESP32-S3 Touch LCD 1.28, ESP-IDF 6.1

## Resumen

Se desarrolló el código fuente de una arquitectura IoT basada en dos microcontroladores ESP32 y un servidor local. El nodo principal dispone de un acelerómetro QMI8658, una interfaz gestual y un modelo LDA que se ejecuta localmente. Un segundo ESP32 recibe comandos mediante ESP-NOW y confirma las operaciones mediante ACK, lo que permite calcular el tiempo de ida y vuelta. El servidor edge registra telemetría, visualiza el estado y publica imágenes de firmware para OTA A/B.

El desarrollo incluye pruebas reproducibles del clasificador y del servidor en un computador. La compilación integrada, el enlace de dos placas, la visualización en LCD y el ciclo OTA físico permanecen **pendientes de validación experimental**. No se presentan registros de esas pruebas como si ya hubieran sido realizados.

## Objetivos

- Ejecutar un algoritmo supervisado en el ESP32-S3 para interpretar posiciones del dispositivo.
- Construir una interfaz HMI mediante inclinaciones de la placa.
- Implementar comunicación nodo-nodo con ESP-NOW y respuestas a nivel de aplicación.
- Implementar comunicación nodo-edge mediante Wi-Fi/HTTP.
- Preparar OTA con dos particiones de aplicación y mecanismo de rollback.
- Mantener el código modular y documentado con Doxygen.

## Arquitectura

```text
Persona -> Inclinaciones -> QMI8658 (I2C)
                                 |
                                 v
                        ESP32-S3 Waveshare
                        LDA + interfaz LCD
                          |           |
                      ESP-NOW     Wi-Fi / HTTP
                          |           |
                          v           v
                     ESP32 receptor  PC edge
                     LED + ACK       dashboard + firmware.bin
```

### Configuración principal

| Parámetro | Valor |
|---|---|
| Sensor IMU | QMI8658, dirección I2C `0x6B` |
| I2C | SDA GPIO6; SCL GPIO7 |
| LCD | GC9A01, resolución 240 x 240 |
| Flash | 16 MB |
| Plataforma de desarrollo | ESP-IDF 6.1 |
| Radio | Wi-Fi 2,4 GHz y ESP-NOW |
| Interfaz humana | Cuatro inclinaciones y posición de reposo |

Los nombres `X_POS`, `X_NEG`, `Y_POS` y `Y_NEG` corresponden a las posiciones etiquetadas durante la captura; no se interpretan automáticamente como signos matemáticos del sensor.

## Adquisición y clasificación

La adquisición se realizó enviando lecturas del acelerómetro por el puerto serie y almacenándolas en `datos_ia/gestos_imu.csv`. Se registraron 915 muestras, distribuidas en 3 rondas, 5 clases y 61 lecturas por clase y ronda.

Se utilizó análisis discriminante lineal (LDA). El modelo obtiene puntuaciones mediante una combinación lineal de `AX`, `AY` y `AZ`, seguido de una función softmax para distinguir las cinco clases. La inferencia se compila en C en el nodo principal: no requiere ejecutar Python en el ESP32.

### Resultados verificados fuera de la placa

| Evaluación | Resultado |
|---|---|
| Calidad del CSV | 915 filas, sin valores faltantes |
| Validación por rondas, cada una dejada fuera del entrenamiento | 100 % en cada ronda |
| Equivalencia del código C sobre las capturas | 915/915 clases correctas en prueba host |
| Protocolo C ESP-NOW | Tamaño de estructura 12 bytes y campos validados |
| API del servidor | POST telemetría, GET historial, persistencia JSONL probados localmente |

**Interpretación:** las métricas representan capturas controladas. No estiman la exactitud ante usuarios distintos, sacudidas o posturas no presentes en el CSV. La prueba del modelo C fue ejecutada en un computador, no en hardware ESP32.

## Gestión de dispositivos y acciones

1. **Encendido remoto:** inclinación etiquetada `X_POS`, comando ESP-NOW al receptor, LED activo.
2. **Apagado remoto:** inclinación `X_NEG`, comando de desconexión del LED.
3. **Consulta del estado:** inclinación `Y_POS`, solicitud de estado, ACK con secuencia y estado reportado.

La inclinación `Y_NEG` sostenida inicia el procedimiento de actualización OTA cuando existe conexión Wi-Fi. La postura de reposo rearma el reconocimiento de comandos.

Los comandos son acciones implementadas en el código. Su ejecución real entre dos placas requiere aún una prueba de banco.

## Protocolos y métricas

| Ruta | Protocolo | Métrica implementada | Evidencia actual |
|---|---|---|---|
| Nodo -> Nodo | ESP-NOW | RTT calculado entre orden y ACK, en milisegundos | Instrumentación en firmware; datos de hardware pendientes |
| Nodo -> Edge | HTTP sobre Wi-Fi | Tiempo de `esp_http_client_perform()` | Instrumentación en firmware; datos de hardware pendientes |
| Edge -> Nodo | HTTP OTA | Estado de descarga y resultado de actualización | Implementación preparada; prueba OTA física pendiente |

La comparación cuantitativa HTTP/MQTT/WebSocket, la tasa de entrega con series repetidas y las mediciones de sobrecarga están descritas como extensiones posibles; **no se informan valores no medidos**.

## OTA y seguridad

La tabla de particiones reserva `ota_0` y `ota_1` para imágenes de aplicación, y `otadata` para seleccionar la imagen activa. El nodo descarga `firmware.bin` desde el servidor edge y puede confirmar el arranque nuevo mediante `esp_ota_mark_app_valid_cancel_rollback()`.

El transporte de la demostración es **HTTP sin TLS** y el canal ESP-NOW no utiliza cifrado. Por tanto, el prototipo solo debe ejecutarse en una red de laboratorio confiable. El trabajo no implementa ni reivindica autenticación criptográfica, Secure Boot, firma de firmware o protección de tráfico frente a terceros.

## Verificación y evidencias por recoger

1. Compilación de ambos proyectos en ESP-IDF 6.1 y captura del mensaje `Project build complete`.
2. Monitores seriales de los dos ESP32 con identificadores de dirección MAC y canal coincidente.
3. Envío de ON, OFF y ESTADO; fotografía del LED y capturas de confirmaciones ACK/RTT.
4. Pantalla GC9A01 mostrando las decisiones del nodo principal.
5. Panel edge recibiendo telemetría con marca de tiempo.
6. Actualización OTA de v1.0.0 a v1.0.1 con registro de reinicio y validación de firmware.

Para replicar las comprobaciones disponibles en el computador: `python pruebas/test_estatico.py` (requiere GCC y dependencias Python indicadas en `datos_ia/requirements.txt`).

## Organización del repositorio

- `nodo_waveshare/`: firmware principal y modelo entrenado.
- `nodo_receptor/`: receptor ESP-NOW.
- `edge_server/`: panel y servidor OTA.
- `datos_ia/`: mediciones, entrenamiento y reporte.
- `pruebas/`: validación de escritorio.
- `documentacion/`: diagramas, análisis técnico, seguridad y procedimiento.
- `Doxyfile`: generación de documentación de código.

## Conclusión

El resultado reúne código fuente modular para adquisición del QMI8658, inferencia supervisada, interacción gestual, intercambio de órdenes por ESP-NOW, telemetría edge y actualización OTA. La reproducibilidad del entrenamiento y las pruebas de escritorio sustentan la parte analítica. La aceptación definitiva del prototipo depende de la compilación y evaluación física descritas en la sección anterior.