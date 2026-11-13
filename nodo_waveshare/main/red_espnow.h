/** @file red_espnow.h @brief Comunicacion nodo-nodo via ESP-NOW. */
#ifndef IOT_NOW_H
#define IOT_NOW_H
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/** @brief Inicia Wi-Fi STA (opcional AP) y ESP-NOW. */
esp_err_t red_iniciar(void);
/** @brief Procesa avisos HELLO / ACK fuera del callback de Wi-Fi. */
void red_procesar(void);
/** @brief Envia comando de encendido/apagado/consulta al receptor. */
esp_err_t red_enviar(uint8_t comando);
/** @brief Indica si se ha descubierto al receptor via HELLO. */
bool red_receptor_descubierto(void);
/** @brief Devuelve estado de la conexion Wi-Fi al punto de acceso. */
bool red_wifi_conectado(void);
/** @brief Devuelve RTT ultimo ACK en milisegundos (-1 sin ACK). */
float red_ultimo_rtt_ms(void);
/** @brief Devuelve estado LED reportado por receptor (-1 desconocido). */
int red_estado_remoto(void);
/** @brief Canal actual del Wi-Fi compartido con ESP-NOW. */
uint8_t red_canal_actual(void);
#endif