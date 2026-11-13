# INICIO RAPIDO — EMPEZAR POR AQUI

## 1. No modificar `hello_world`

Extraer todo el ZIP en `C:\Users\santi\Proyecto_IoT_ESP32_Final`.
En una ventana **nueva** de VS Code abrir **solo** `nodo_waveshare`.

## 2. Waveshare ESP32-S3 por COM12

Desde terminal **ESP-IDF**:

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\nodo_waveshare
idf.py set-target esp32s3
idf.py build
idf.py -p COM12 flash monitor
```

La primera compilacion requiere Internet para instalar el componente
`espressif/esp_lcd_gc9a01` v2.0.x. Si falla, enviar el mensaje que comienza
con `error:`, `FAILED` o `CMake Error` (no reinstalar ESP-IDF).

El monitor debe mostrar `QMI8658 detectado`, `ESP_NOW_S3: CANAL ACTUAL`,
y la IA calculando `IA=...` cada 1.5s. La pantalla debe mostrar un titulo.

## 3. Encender segundo ESP32 y cargar receptor

En otra ventana de VS Code, abrir **solo** `nodo_receptor`.
Cambiar COM13 si el puerto detectado es otro, y cambiar `esp32` a `esp32s3`
si el segundo chip es ESP32-S3.

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\nodo_receptor
idf.py set-target esp32
idf.py build
idf.py -p COM13 flash monitor
```

Si la Waveshare esta asociada al AP, copiar el canal mostrado por la Waveshare
a `nodo_receptor/main/configuracion_receptor.h` (`IOT_ESPNOW_CANAL`).

## 4. Comandos gestuales

Desde las mismas posiciones que capturaste para el CSV:

- X_POS = encender LED remoto
- X_NEG = apagar LED remoto
- Y_POS = consultar estado
- Y_NEG = mantener 3.5 segundos para OTA
- REPOSO = volver a armar el clasificador

`ACK seq=... LED=... RTT=...ms` comprueba que responde el segundo ESP32.

## 5. Conectar nodo-edge / OTA

Lee el `README.md` de la raiz para configurar Wi-Fi, servidor edge y
version 1.0.1 por OTA. Las claves reales van en `configuracion_local.h`,
que Git ignora.

## 6. Entrega final

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final
doxygen Doxyfile
git init
git add .
git commit -m "Proyecto IoT ESP32-S3 completo"
```

Despues crear repositorio vacio en GitHub, asociar remoto y hacer `git push`.
No subir claves privadas ni `build/`.

## Estado comprobado (sin exagerar)

Las pruebas locales del modelo C y servidor Python estan en
`pruebas/RESULTADOS_PRUEBAS_HOST.txt`. Todavia falta comprobar el firmware
integrado en ESP-IDF, las dos placas y la actualizacion OTA fisica.