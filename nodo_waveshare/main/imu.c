/**
 * @file imu.c
 * @brief Driver I2C QMI8658 para Waveshare ESP32-S3-Touch-LCD-1.28.
 * @details GPIO6 SDA; GPIO7 SCL; direccion 0x6B confirmada fisicamente.
 */
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "imu.h"

#define IMU_ADDR 0x6B
#define WHO_AM_I 0x00
#define CTRL1 0x02
#define CTRL2 0x03
#define CTRL7 0x08
#define ACC_X_L 0x35

static const char *TAG = "IMU";
static i2c_master_dev_handle_t s_sensor = NULL;

/** @brief Escribe registro QMI8658. */
static esp_err_t escribir(uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_sensor, buf, sizeof(buf), 1000);
}

/** @brief Lee registros contiguos QMI8658. */
static esp_err_t leer(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_sensor, &reg, 1, buf, len, 1000);
}

esp_err_t imu_iniciar(void)
{
    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 6,
        .scl_io_num = 7,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t ret = i2c_new_master_bus(&cfg, &bus);
    if (ret != ESP_OK) return ret;

    const i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IMU_ADDR,
        .scl_speed_hz = 100000
    };
    ret = i2c_master_bus_add_device(bus, &dev, &s_sensor);
    if (ret != ESP_OK) return ret;

    uint8_t id = 0;
    ret = leer(WHO_AM_I, &id, 1);
    if (ret != ESP_OK) return ret;
    if (id != 0x05) {
        ESP_LOGE(TAG, "WHO_AM_I inesperado 0x%02X", id);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Secuencia verificada durante las pruebas anteriores en esta placa. */
    ret = escribir(CTRL7, 0x00);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(20));
    ret = escribir(CTRL1, 0x40);
    if (ret != ESP_OK) return ret;
    ret = escribir(CTRL2, 0x15); /* +/-4 g, 250 Hz */
    if (ret != ESP_OK) return ret;
    ret = escribir(CTRL7, 0x01); /* Habilitar acelerometro */
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "QMI8658 detectado ID=0x%02X, AX/AY/AZ activos", id);
    return ESP_OK;
}

esp_err_t imu_aceleraciones(float *ax, float *ay, float *az)
{
    if (!ax || !ay || !az) return ESP_ERR_INVALID_ARG;
    uint8_t b[6];
    esp_err_t ret = leer(ACC_X_L, b, sizeof(b));
    if (ret != ESP_OK) return ret;
    const int16_t x = (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    const int16_t y = (int16_t)((uint16_t)b[2] | ((uint16_t)b[3] << 8));
    const int16_t z = (int16_t)((uint16_t)b[4] | ((uint16_t)b[5] << 8));
    *ax = x / 8192.0f;
    *ay = y / 8192.0f;
    *az = z / 8192.0f;
    return ESP_OK;
}