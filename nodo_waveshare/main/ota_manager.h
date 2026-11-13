/** @file ota_manager.h @brief OTA y confirmacion de rollback. */
#ifndef IOT_OTA_H
#define IOT_OTA_H
#include "esp_err.h"
/** @brief Confirma el firmware si esta pendiente de verificacion de arranque. */
void ota_confirmar_arranque(void);
/** @brief Descarga y aplica firmware desde PC; reinicia cuando es exitoso. */
esp_err_t ota_ejecutar(void);
#endif