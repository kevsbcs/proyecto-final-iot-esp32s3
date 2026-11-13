/** @file red_edge.h @brief Telemetria nodo-edge por HTTP LAN. */
#ifndef IOT_EDGE_H
#define IOT_EDGE_H
#include "esp_err.h"
/** @brief Publica estado de sensores e IA al PC edge. */
esp_err_t edge_publicar(float ax, float ay, float az,
                       const char *gesto, float confianza,
                       int led, float rtt_espnow_ms);
/** @brief Ultimo RTT del POST HTTP en ms, -1 sin medicion. */
float edge_ultimo_rtt_ms(void);
#endif