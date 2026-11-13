# Sistema IoT con ESP32-S3: gestos, IA y comunicación inalámbrica

Prototipo de gestión remota de dispositivos desarrollado en ESP-IDF 6.1.

## Descripción

El nodo principal (Waveshare ESP32-S3 Touch LCD 1.28) lee el acelerómetro QMI8658 y clasifica cinco posiciones mediante un modelo LDA entrenado con mediciones de la placa. Las órdenes se envían por ESP-NOW a un segundo ESP32, que controla un LED y devuelve confirmaciones para medir el tiempo de respuesta. Una aplicación Python en un computador recibe telemetría HTTP, muestra un panel de seguimiento y distribuye imágenes de firmware para la actualización OTA con particiones A/B.

## Componentes

- **Nodo Waveshare:** lectura I2C, inferencia local, comandos por gestos, ESP-NOW, telemetría y OTA.
- **Nodo receptor:** ejecución de órdenes, estado del LED y ACK con número de secuencia.
- **Servidor edge:** recepción de datos, historial JSONL, panel web y distribución de firmware.
- **Entrenamiento:** CSV de capturas del QMI8658, script reproducible y reporte de clasificación.
- **Documentación:** arquitectura, análisis de los ejemplos proporcionados en clase, Doxygen y plan de pruebas.

## Preparación

Abrir cada subproyecto (`nodo_waveshare` y `nodo_receptor`) por separado en VS Code con ESP-IDF 6.1. La Waveshare utiliza la configuración de 16 MB de flash. Configurar credenciales locales en `nodo_waveshare/main/configuracion_local.h`, creado a partir del archivo `.ejemplo`, **sin incluir contraseñas en Git**.

Para el nodo principal:

```powershell
cd nodo_waveshare
idf.py set-target esp32s3
idf.py build
idf.py -p COM12 flash monitor
```

Para el receptor, usar el target y el puerto COM que correspondan a la segunda placa:

```powershell
cd nodo_receptor
idf.py set-target esp32
idf.py build
idf.py -p COM13 flash monitor
```

Servidor en el PC:

```powershell
cd edge_server
python servidor_edge.py --port 8000
```

## Pruebas y alcance

Las capturas y el entrenamiento del clasificador se evaluaron por rondas; las comprobaciones locales del modelo C y de la API Python se realizan con `python pruebas/test_estatico.py`. La compilación de la versión integrada en ESP-IDF, el intercambio inalámbrico entre placas y la OTA en hardware deben verificarse en el banco de laboratorio antes de reportarse como pruebas aprobadas.

**Seguridad:** la versión de práctica utiliza HTTP sin TLS y ESP-NOW sin cifrado; debe ejecutarse exclusivamente en una red local de laboratorio. El firmware no está diseñado para accionar equipos conectados a tensión de red.

Consulta el paquete de fuentes del proyecto y la guía completa para instrucciones de despliegue, instrumentación y evidencias.
