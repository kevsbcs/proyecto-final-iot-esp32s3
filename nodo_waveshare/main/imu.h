/** @file imu.h @brief Adquisicion QMI8658 por I2C. */
#ifndef IOT_IMU_H
#define IOT_IMU_H
#include "esp_err.h"
/** @brief Inicializa el QMI8658 a +/-4 g y 250 Hz. */
esp_err_t imu_iniciar(void);
/** @brief Lee aceleraciones en unidades de gravedad [g]. */
esp_err_t imu_aceleraciones(float *ax, float *ay, float *az);
#endif