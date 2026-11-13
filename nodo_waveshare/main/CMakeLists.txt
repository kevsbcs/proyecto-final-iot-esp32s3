idf_component_register(
    SRCS "main.c" "imu.c" "modelo_ia.c" "red_espnow.c" "red_edge.c" "ota_manager.c" "pantalla.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_i2c esp_driver_gpio esp_driver_spi esp_lcd esp_lcd_gc9a01
                  esp_wifi esp_netif esp_event nvs_flash esp_timer
                  esp_http_client esp_https_ota app_update
)