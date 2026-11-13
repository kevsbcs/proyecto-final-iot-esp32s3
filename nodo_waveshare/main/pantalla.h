/** @file pantalla.h @brief Estado visual en LCD redonda GC9A01. */
#ifndef IOT_PANTALLA_H
#define IOT_PANTALLA_H
#include "esp_err.h"
/** @brief Inicializa LCD 240x240 SPI con backlight GPIO2. */
esp_err_t pantalla_iniciar(void);
/** @brief Cambia estado y detalle en la pantalla redonda. */
void pantalla_mostrar(const char *estado, const char *detalle);
#endif