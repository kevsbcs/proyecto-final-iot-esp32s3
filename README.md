# Proyecto final IoT — ESP32-S3 con IA, gestos, ESP-NOW y OTA

**Objetivo:** satisfacer los cuatro requisitos del ingeniero mediante un
prototipo verificable: (1) OTA, (2) IA ejecutada en el ESP32, (3) HMI por
inclinaciones y LCD, (4) red nodo-nodo y nodo-edge. **Firmware fuente
preparado pero no compilado ni verificado en hardware en este entorno**;
siga las pruebas indicadas para confirmar la entrega.

## Hardware que ya se verifico

- **Nodo principal:** Waveshare ESP32-S3 Touch LCD 1.28, 16 MB flash, IMU
  QMI8658 (I2C SDA=GPIO6, SCL=GPIO7; direccion 0x6B; WHO_AM_I=0x05),
  pantalla GC9A01A 240x240 (CLK10 MOSI11 CS9 DC8 RST14 BL2).
- **Nodo receptor:** un segundo ESP32 (se asume ESP32 DevKit ESP32-WROOM);
  LED interno usualmente GPIO2. Si es ESP32-S3, cambie `idf.py set-target`.
- **PC edge:** Windows, Python 3.10+, conectado a la misma Wi-Fi 2.4 GHz que la
  Waveshare para HTTP; USB para la primera carga de firmware.
- **Herramientas del usuario:** Windows, VS Code, ESP-IDF v6.1; Waveshare en
  **COM12** durante las pruebas previas. La segunda placa aparecera como otro COM.

**Informe técnico:** [documentacion/INFORME_PRACTICA.md](documentacion/INFORME_PRACTICA.md) documenta metodología, resultados verificables y pruebas físicas pendientes. La carpeta `.github/workflows` ejecuta pruebas host automatizadas en GitHub Actions.

## Estructura de carpetas

```text
Proyecto_IoT_ESP32_Final/
├── README.md
├── .gitignore
├── Doxyfile
├── nodo_waveshare/               # Proyecto ESP-IDF del ESP32-S3
│   ├── CMakeLists.txt
│   ├── partitions.csv           # OTA_0 / OTA_1, flash 16 MB
│   ├── sdkconfig.defaults
│   └── main/
│       ├── CMakeLists.txt
│       ├── idf_component.yml    # Driver de LCD GC9A01
│       ├── main.c
│       ├── configuracion.h
│       ├── configuracion_local.h.ejemplo
│       ├── imu.[ch]
│       ├── modelo_ia.[ch]
│       ├── modelo_gestos.h
│       ├── red_espnow.[ch]
│       ├── red_edge.[ch]
│       ├── ota_manager.[ch]
│       ├── pantalla.[ch]
│       ├── fuente5x7.h
│       └── protocolo.h
├── nodo_receptor/                # Proyecto ESP-IDF segundo ESP32
│   ├── CMakeLists.txt
│   └── main/
│       ├── CMakeLists.txt
│       ├── main.c
│       ├── configuracion_receptor.h
│       └── protocolo.h
├── edge_server/
│   ├── servidor_edge.py         # API y dashboard HTTP
│   └── publicar_firmware.py
├── datos_ia/
│   ├── gestos_imu.csv           # 915 muestras reales
│   ├── entrenar_modelo.py
│   ├── reporte_modelo.txt
│   └── requirements.txt
├── protocolo/protocolo.h
├── documentacion/
│   ├── ARQUITECTURA.md
│   ├── ANALISIS_MATERIALES.md
│   └── SEGURIDAD_Y_PRUEBAS.md
└── pruebas/
    └── test_estatico.py
```

## PASO 1 — Extraer en Windows y compilar Waveshare

**No sobrescriba `hello_world`**: conservamos esa carpeta como respaldo, y
usamos una nueva para evitar el anterior error de `app_main` o CMake de Arduino.

1. Extraer este ZIP, por ejemplo en `C:\Users\santi\Proyecto_IoT_ESP32_Final`.
2. Conectar la Waveshare por USB y abrir en VS Code la carpeta
   `Proyecto_IoT_ESP32_Final\nodo_waveshare` (no la raíz general).
3. Abrir una terminal **ESP-IDF** con `Ctrl+Shift+P` →
   `ESP-IDF: Open ESP-IDF Terminal`.
4. Ejecutar:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\nodo_waveshare
idf.py set-target esp32s3
idf.py build
idf.py -p COM12 flash monitor
```

En la primera compilación, el gestor de componentes descargará
`espressif/esp_lcd_gc9a01` (requiere Internet). No es necesario añadir LVGL ni
Arduino. El proyecto usa un framebuffer ligero y una fuente de 5x7 propia.
En caso de error con el driver LCD, debe diagnosticarse la causa: no sustituir
pines ni añadir dependencias de otra placa por intuición.

**Esperado en monitor:** `QMI8658 detectado`, `IoT ESP32-S3 v1.0.0`,
`CANAL ACTUAL: 1`, `IA=REPOSO`, y la LCD muestra `LISTO`.
Sin receptor ni Wi-Fi la IA y la pantalla siguen funcionando, y se verá
`Esperando HELLO ...` en los logs.

## PASO 2 — Segundo ESP32 receptor (nodo-nodo)

1. Conectar el segundo ESP32 al USB y ubicar su puerto en Administrador de
   dispositivos. En los ejemplos usamos **COM13** como ilustración, NO como
   puerto confirmado.
2. Abrir en **otra ventana VS Code** la carpeta `nodo_receptor`.
3. Editar `main/configuracion_receptor.h` si el LED está en otro pin.
4. Si la Waveshare no se conectó a un router, ambos usan canal 1 por defecto.
   Si sí se conectó, el receptor debe usar el canal **impreso por la Waveshare**.
5. Ejecutar, adaptando puerto y target:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\nodo_receptor
idf.py set-target esp32
idf.py build
idf.py -p COM13 flash monitor
```

La Waveshare recibe HELLO de difusión, aprende la MAC sin editar direcciones
hardcodeadas y envía órdenes unicast. El receptor devuelve ACK y su estado.
El **cambio visible del LED** depende de si su placa incorpora un LED en
GPIO2; si no lo tiene, conectar un LED externo con resistencia apropiada y
cambiar el GPIO. No conectar relés de red eléctrica para esta práctica.

### Gestos entrenados (no confundir etiqueta y signo numérico)

| Gesto físico etiquetado | Acción real |
|-------------------------|-------------|
| `REPOSO` | Rearma el detector |
| `X_POS` | LED remoto ENCENDIDO |
| `X_NEG` | LED remoto APAGADO |
| `Y_POS` | Preguntar estado LED al receptor |
| `Y_NEG` mantenido ~3.5 segundos | Iniciar OTA si Wi-Fi conectado |

`X_POS` y `X_NEG` son etiquetas de **posturas físicas** en la recolección, no
necesariamente valores AX positivos/negativos: los valores de los ejes dependen
de la posición y del montaje. Repetir la inclinación con que se capturó cada
etiqueta. El firmware filtra sacudidas y exige 4 predicciones consecutivas;
regresar a REPOSO entre comandos.

## PASO 3 — Nodo-edge por Wi-Fi (computador)

1. PC y Waveshare deben compartir Wi-Fi de **2.4 GHz**. El ESP32-S3 no se
   conecta a una red 5 GHz.
2. En PowerShell en el PC, ejecutar `ipconfig` para obtener IPv4 de la Wi-Fi,
   por ejemplo `192.168.1.100` (sólo ejemplo).
3. Copiar `nodo_waveshare/main/configuracion_local.h.ejemplo` como
   `nodo_waveshare/main/configuracion_local.h` (archivo ignorado en Git).
   Editar con sus **valores reales**:

```c
#define IOT_WIFI_SSID "MI_RED_2G"
#define IOT_WIFI_PASS "MI_CONTRASENA"
#define IOT_EDGE_IP "192.168.1.100"
```

4. En la terminal ESP-IDF de la Waveshare:

```powershell
idf.py build
idf.py -p COM12 flash monitor
```

5. En otra terminal (Python normal) ejecutar:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\edge_server
python servidor_edge.py --port 8000
```

6. Abrir en el PC **http://localhost:8000**. Permitir el puerto TCP 8000 en
   el cortafuegos de Windows **solo en el perfil de red Privada**.
7. La terminal del ESP32 debe mostrar `Wi-Fi asociado`, `POST OK`, y el panel
   del PC tendrá las aceleraciones, gestos, estado LED y RTT ESP-NOW.

**Importante:** al asociarse a un router, la radio de la Waveshare pasa al
canal del router. El receptor ESP-NOW debe usar **ese mismo canal**. Si falla
el descubrimiento, mirar `CANAL ACTUAL` en el monitor de la Waveshare y cambiar
`IOT_ESPNOW_CANAL` en `nodo_receptor/main/configuracion_receptor.h`, recompilar
y cargar el receptor. En caso de que cambie el canal del router, repetir.

## PASO 4 — Demostración OTA real con particiones A/B

Esta prueba requiere haber completado la conexión del PASO 3. De lo contrario,
OTA se rechazará de forma segura. Todo OTA se hace en la **red local HTTP sin
TLS por razones de facilidad de demostración**; no es seguro en producción.

1. Asegurarse de que en `nodo_waveshare/main/configuracion.h` figura
   `#define IOT_VERSION_FIRMWARE "1.0.0"` y que la placa **ya ejecuta** esa
   versión. **NO utilizar `flash` para cargar v1.0.1**, pues queremos probar OTA.
2. Cambiar en el archivo completo `configuracion.h` solo el valor de la macro
   de versión a `"1.0.1"` (el resto se mantiene). Ejecutar:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\nodo_waveshare
idf.py build
```

3. Copiar el nuevo binario al servidor (terminal Python):

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\edge_server
python publicar_firmware.py ..\nodo_waveshare\build\iot_waveshare.bin
python servidor_edge.py --port 8000
```

4. Comprobar desde el navegador del PC:
   `http://localhost:8000/firmware.bin` debe ofrecer un archivo binario.
5. Mover la Waveshare al gesto entrenado `Y_NEG` y mantenerlo **3.5 s**. Debe
   aparecer `Comenzando OTA`, descargar, cambiar `ota_0`/`ota_1` y reiniciar.
6. Tras reiniciar, comprobar `IoT ESP32-S3 v1.0.1` y
   `Firmware nuevo VALIDADO; rollback confirmado`.

**ATENCIÓN:** para poder probar de nuevo desde 1.0.0 habrá que cargar por USB
la versión anterior. Durante una actualización NO apagar el dispositivo. Si
el servidor responde 404 o no existe Wi-Fi, la función OTA informa error y
mantiene la versión anterior.

## PASO 5 — Verificar/entrenar nuevamente el modelo de IA

Este repositorio contiene las mediciones reales `datos_ia/gestos_imu.csv`,
el entrenamiento reproducible y la matriz de confusión:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\datos_ia
python -m pip install -r requirements.txt
python entrenar_modelo.py --csv gestos_imu.csv --salida ..\nodo_waveshare\main\modelo_gestos.h --reporte reporte_modelo.txt
```

- 915 filas = 3 rondas x 5 gestos x 61 lecturas; sin faltantes.
- Entrenamiento: LDA supervisado; modelo final entrenado con todas las filas.
- Validación cruzada por rondas: 100 % sobre las 3 poses capturadas. **No**
  extrapolar esa exactitud a movimientos no capturados o a nuevas personas.
- La inferencia en C es `W*AX,AY,AZ + b` seguido de softmax y filtros.
- **No se necesita TensorFlow Lite para desplegar este clasificador lineal**;
  el notebook `tinyml_01.ipynb` del ingeniero muestra un flujo alternativo.

## PASO 6 — Documentar con Doxygen y subir a GitHub

Instalar Doxygen desde su sitio oficial y, desde la raíz de este repositorio:

```powershell
doxygen Doxyfile
```

Abrir `documentacion/doxygen_generado/html/index.html` con el navegador.
No se incluyen carpetas `build`, `.vscode`, SDK ni credenciales en el repositorio.

Desde PowerShell con Git instalado:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final
git init
git add .
git commit -m "Proyecto IoT ESP32-S3 IA gestos ESP-NOW OTA edge"
git branch -M main
git remote add origin https://github.com/kevsbcs/proyecto-final-iot-esp32s3.git
git push -u origin main
```

Crear primero el repositorio vacío en GitHub y sustituir la URL por la propia.
El código **no** se sube a GitHub automáticamente desde este ZIP.

## Evidencias que debe obtener el equipo

| Evidencia | Qué debe aparecer |
|----------|-------------------|
| IA en dispositivo | `IA=...` con 5 clases y pantalla LCD |
| Nodo-nodo | `RECEPTOR DESCUBIERTO`, `CMD`, ACK con RTT y LED cambiando |
| Nodo-edge | Dashboard con nuevos datos cada 6 s, `POST OK` |
| OTA | Registro de versión v1.0.0→v1.0.1 y validación post-OTA |
| Documentación | HTML generado por Doxygen, GitHub con fuentes |

### Estado verificable

**Validado en pruebas previas del usuario:** arranque ESP32-S3 (16 MB), lectura
IMU 0x6B, cambios de aceleración en movimiento, adquisición de CSV, 915 datos
y entrenamiento Python. **Aún pendiente de verificación física y de
compilación:** combinación total de módulos, pantalla, segundo ESP32,
enlace ESP-NOW, edge y OTA. No afirmar que funcionan hasta hacer las pruebas.

## Análisis y seguridad

Véanse `documentacion/ANALISIS_MATERIALES.md` y
`documentacion/SEGURIDAD_Y_PRUEBAS.md`. Los ejemplos del ingeniero son
referencia académica, no deben copiarse sin adaptar las direcciones y pines.