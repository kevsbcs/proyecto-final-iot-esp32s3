# Proyecto final IoT — ESP32-S3

**Código fuente documentado con Doxygen, desarrollado en ESP-IDF 6.1 y organizado según los cuatro puntos solicitados en clase.**

## Requisitos de la práctica

| Punto solicitado | Implementación | Archivos principales |
|---|---|---|
| **1. OTA** | Actualización del firmware del ESP32-S3 por Wi-Fi desde un servidor local, con particiones `ota_0` y `ota_1`. | [ota_manager.c](nodo_waveshare/main/ota_manager.c), [partitions.csv](nodo_waveshare/partitions.csv), [publicar_firmware.py](edge_server/publicar_firmware.py) |
| **2. Inteligencia artificial en el ESP32 para gestión de dispositivos** | Clasificador LDA entrenado con aceleraciones del QMI8658. La inferencia se ejecuta en el ESP32-S3 y decide qué orden enviar al dispositivo remoto. | [modelo_ia.c](nodo_waveshare/main/modelo_ia.c), [modelo_gestos.h](nodo_waveshare/main/modelo_gestos.h), [entrenar_modelo.py](datos_ia/entrenar_modelo.py) |
| **3. Interfaz de voz o gestos para interacción humano-computador** | Interfaz por **gestos**: inclinaciones reconocidas mediante el acelerómetro y visualizadas en la pantalla circular. | [main.c](nodo_waveshare/main/main.c), [imu.c](nodo_waveshare/main/imu.c), [pantalla.c](nodo_waveshare/main/pantalla.c) |
| **4. Arquitectura de red nodo-edge y nodo-nodo** | **Nodo-nodo:** ESP-NOW entre la Waveshare y un segundo ESP32. **Nodo-edge:** Wi-Fi/HTTP entre la Waveshare y un servidor Python en el computador. | [red_espnow.c](nodo_waveshare/main/red_espnow.c), [receptor/main.c](nodo_receptor/main/main.c), [red_edge.c](nodo_waveshare/main/red_edge.c), [servidor_edge.py](edge_server/servidor_edge.py) |

No se implementa interfaz de voz: la consigna permite **voz o gestos** y se eligió la segunda opción. El dashboard del servidor sirve para visualizar la comunicación nodo-edge; no es un requisito independiente.

## Funcionamiento

```text
Persona
  └─ Inclinación / gesto
       └─ Waveshare ESP32-S3
            ├─ QMI8658 → clasificación de IA local
            ├─ Pantalla LCD → gesto y estado
            ├─ ESP-NOW → segundo ESP32 → LED / respuesta
            └─ Wi-Fi HTTP → computador edge
                               ├─ telemetría
                               └─ descarga de firmware para OTA
```

El entrenamiento utilizó [915 muestras etiquetadas](datos_ia/gestos_imu.csv) en cinco clases: `REPOSO`, `X_POS`, `X_NEG`, `Y_POS` y `Y_NEG`. Los parámetros del modelo se exportaron a C para realizar las predicciones en la placa, sin depender de Python durante la inferencia.

Las acciones previstas son: `X_POS` encender LED remoto, `X_NEG` apagar, `Y_POS` consultar estado y `Y_NEG` sostenido solicitar OTA. Las etiquetas corresponden a las posturas usadas en la captura.

## Estructura de la entrega

- [`nodo_waveshare/`](nodo_waveshare): firmware del nodo inteligente ESP32-S3, IA, HMI, comunicaciones y OTA.
- [`nodo_receptor/`](nodo_receptor): segundo ESP32 que recibe órdenes por ESP-NOW.
- [`edge_server/`](edge_server): servicio Python para comunicación nodo-edge y firmware OTA.
- [`datos_ia/`](datos_ia): capturas, entrenamiento y evaluación del clasificador.
- [`protocolo/`](protocolo): definición de las tramas compartidas entre los nodos.
- [`Doxyfile`](Doxyfile): configuración de documentación Doxygen.
- [`documentacion/`](documentacion): arquitectura e información técnica complementaria.

## Compilación y ejecución

Se requiere **VS Code con ESP-IDF 6.1**. Compilar por separado cada firmware:

**Nodo principal Waveshare ESP32-S3:**

```powershell
cd nodo_waveshare
idf.py set-target esp32s3
idf.py build
idf.py -p COM12 flash monitor
```

**Nodo receptor ESP32 DevKit (ajustar placa y puerto):**

```powershell
cd nodo_receptor
idf.py set-target esp32
idf.py build
idf.py -p COM13 flash monitor
```

Para habilitar el servidor edge y OTA, copiar `nodo_waveshare/main/configuracion_local.h.ejemplo` como `configuracion_local.h` y configurar el SSID de 2,4 GHz, la contraseña y la IP local del computador. Ese archivo privado está excluido de Git.

```powershell
cd edge_server
python servidor_edge.py --port 8000
```

El nodo receptor debe usar el mismo canal inalámbrico que el nodo Waveshare. La configuración predeterminada del receptor es canal 1; cuando la Waveshare se asocia a un router, se debe adaptar al canal de esa red.

Para OTA, se usa el [procedimiento de publicación](edge_server/README.md) del binario compilado. Las particiones A/B están definidas en el archivo CSV del nodo principal. La demostración utiliza HTTP en una red local de laboratorio, **sin TLS**.

## Documentación Doxygen

Los archivos C incluyen bloques `@file`, `@brief`, `@param` y comentarios de funciones. Desde la raíz del repositorio:

```powershell
doxygen Doxyfile
```

La documentación HTML se genera en `documentacion/doxygen_generado/html/index.html`.

## Alcance de la verificación

**Realizado:** captura de datos del QMI8658, entrenamiento y evaluación del modelo con muestras registradas, preparación del código y publicación del repositorio.

**Pendiente en hardware:** compilar esta versión integrada en ESP-IDF 6.1 y verificar con dos placas las órdenes ESP-NOW, el enlace HTTP y una actualización OTA completa. Estos ensayos son útiles para sustentar el funcionamiento, pero **no se enumeran como entregables adicionales** en la consigna compartida.

La documentación complementaria está en [documentacion/INFORME_PRACTICA.md](documentacion/INFORME_PRACTICA.md).
