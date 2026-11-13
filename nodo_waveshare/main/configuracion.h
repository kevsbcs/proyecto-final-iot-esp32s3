/**
 * @file configuracion.h
 * @brief Parametros generales, con sobreescrituras opcionales privadas.
 *
 * NO subir contrasenas a GitHub. Copie configuracion_local.h.ejemplo
 * a configuracion_local.h para establecer sus credenciales privadas.
 */
#ifndef CONFIGURACION_IOT_H
#define CONFIGURACION_IOT_H

/* Para demostrar OTA: primera compilacion 1.0.0, segunda 1.0.1. */
#define IOT_VERSION_FIRMWARE "1.0.0"

#if defined(__has_include)
# if __has_include("configuracion_local.h")
#  include "configuracion_local.h"
# endif
#endif

#ifndef IOT_WIFI_SSID
#define IOT_WIFI_SSID     ""
#endif
#ifndef IOT_WIFI_PASS
#define IOT_WIFI_PASS     ""
#endif
#ifndef IOT_EDGE_IP
#define IOT_EDGE_IP       "192.168.1.100"
#endif
#ifndef IOT_ESPNOW_CANAL
#define IOT_ESPNOW_CANAL  1
#endif
#ifndef IOT_USAR_PANTALLA
#define IOT_USAR_PANTALLA 1
#endif

/* HTTPS seria necesario fuera del laboratorio; estas URLs son HTTP LAN. */
#define IOT_URL_TELEMETRIA "http://" IOT_EDGE_IP ":8000/api/telemetria"
#define IOT_URL_FIRMWARE   "http://" IOT_EDGE_IP ":8000/firmware.bin"

#endif