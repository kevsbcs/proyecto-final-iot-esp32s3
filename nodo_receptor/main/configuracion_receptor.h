/** @file configuracion_receptor.h @brief Ajustes de placa receptora. */
#ifndef CONFIG_RECEPTOR_H
#define CONFIG_RECEPTOR_H

/* Modelo clasico ESP32 DevKit: LED interno suele estar en GPIO2.
   Si tu placa usa otro GPIO, cambialo aqui. */
#define IOT_LED_GPIO 2
#define IOT_LED_ACTIVO_ALTO 1

/* Debe coincidir con el canal actual que imprime la Waveshare.
   Si la Waveshare conecta a Wi-Fi, este canal debe ser el del router. */
#define IOT_ESPNOW_CANAL 1

#endif