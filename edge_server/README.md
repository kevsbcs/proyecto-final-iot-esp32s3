# Servidor edge (computador Windows)

Sin dependencias externas. Python 3.10 o superior.

```powershell
cd C:\Users\santi\Proyecto_IoT_ESP32_Final\edge_server
python servidor_edge.py --port 8000
```

Abrir **http://localhost:8000**. Para descubrir la dirección IP LAN del PC,
utilizar `ipconfig` (dirección IPv4 de la Wi-Fi). Copiarla a
`nodo_waveshare/main/configuracion_local.h` en `IOT_EDGE_IP`.

El ordenador y la ESP32-S3 deben estar conectados a la misma red de **2.4 GHz**
y el cortafuegos de Windows debe permitir TCP/8000 en el perfil **Privado**.

La API `POST /api/telemetria` admite JSON y lo registra como JSONL. La página
muestra estados IA, LED remoto y RTT en milisegundos. El endpoint
`GET /firmware.bin` sirve un binario OTA cuando se ha publicado con
`publicar_firmware.py`.

**Seguridad:** HTTP está intencionadamente sin TLS para esta demostración;
solo usar en LAN de confianza. El proyecto no implementa autenticación de
usuarios, no debe exponerse a Internet, y no se deben conectar cargas peligrosas.