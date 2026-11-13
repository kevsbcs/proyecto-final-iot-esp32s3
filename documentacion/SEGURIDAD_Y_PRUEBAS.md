# Seguridad, riesgos y protocolo de aceptación

## Alcance y límites honestos

- El firmware implementa descarga OTA y actualización de particiones A/B;
  **no** implementa Secure Boot v2 ni firma criptográfica de imagen.
- La demostración OTA utiliza `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=y` para que
  un servidor Python local sirva `firmware.bin` sin certificados. Aunque se
  usa la API `esp_https_ota`, **la transferencia de demostración es HTTP**,
  no HTTPS y no es segura frente a adversarios en la red.
- ESP-NOW no tiene cifrado ni autenticación de emisor: un transmisor no
  autorizado en el mismo canal podría enviar comandos falsificados. No usar
  para controlar dispositivos peligrosos ni en instalaciones reales.
- No se utilizaron JWT, TLS, mTLS, MQTT seguro, HMAC ni certificados.
- El PC escucha en `0.0.0.0:8000` cuando se ejecuta el servidor; es
  indispensable limitar acceso mediante firewall de perfil **Privado**.
- El almacén `telemetria.jsonl` no se añade a Git. El archivo
  `configuracion_local.h` con claves se excluye de Git automáticamente.
- Las probabilidades LDA provienen de softmax lineal no calibrado, y no
  ofrecen una medida de seguridad para autorizar operaciones críticas.
- La IA puede confundir posiciones transitorias no vistas en el entrenamiento.
  La actualización OTA requiere mantener el gesto 3.5 s para reducir errores,
  pero no reemplaza autenticación o una confirmación física adicional.

## Mejoras para producción (fuera del alcance validado)

1. Configurar un servidor HTTPS con certificado CA verificable, hostname y SAN
   válidos; desactivar `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP`.
2. Activar cifrado ESP-NOW mediante PMK/LMK únicas por par y emparejamiento
   autenticado, con protección frente a repetición.
3. Activar Secure Boot v2, firma de firmware y cifrado de flash donde proceda.
4. Usar retención de logs y control de acceso al dashboard.
5. Implementar timeouts, retransmisiones y seguimiento de secuencias pendientes
   si se exige medir PDR/tasa de entrega con rigor.

## Lista de pruebas de campo a registrar

| ID | Caso | Procedimiento | Éxito esperado | Resultado real |
|---|---|---|---|---|
| T01 | Sensor IMU | Reiniciar WAVESHARE, mover placa | Cambian AX/AY/AZ | Ya comprobado previamente |
| T02 | Inferencia IA | Repetir las 5 poses del CSV | IA reconoce cada clase | Pendiente en firmware integrado |
| T03 | LCD GC9A01 | Iniciar la Waveshare | Texto y color en pantalla | Pendiente |
| T04 | Descubrimiento | Encender receptor en el mismo canal | RECEPTOR DESCUBIERTO | Pendiente |
| T05 | Control LED | X_POS y X_NEG, volver a REPOSO | LED responde y ACK con RTT | Pendiente |
| T06 | Estado remoto | Gesto Y_POS | ACK con estado de LED | Pendiente |
| T07 | Nodo-edge | Wi-Fi conectado, PC server iniciado | Dashboard actualiza datos | Pendiente |
| T08 | OTA | Versiones 1.0.0 → 1.0.1 | Reinicio y valida firmware nuevo | Pendiente |
| T09 | Doxygen | Ejecutar doxygen Doxyfile | HTML index.html | Pendiente |
| T10 | GitHub | Push a repo de estudiante | Archivos fuente visibles | Pendiente |

### Evidencias sugeridas

Capturar por separado:

- Captura del monitor `IA=REPOSO`, `IA=X_POS` y `IA=Y_NEG`.
- Foto de LCD mostrando texto de estado.
- Video breve donde gesto remoto cambia el LED del segundo ESP32.
- Monitor S3 con `ACK seq=... RTT=...ms`.
- Dashboard del servidor edge con datos y versión.
- Secuencia de logs OTA v1.0.0 → v1.0.1, reinicio y confirmación de rollback.
- Captura de repositorio GitHub y página HTML de Doxygen.

**No marcar como validado algo que todavía no se probó físicamente.**