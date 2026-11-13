/**
 * @file ota_manager.c
 * @brief Actualizacion OTA mediante esp_https_ota y rollback del bootloader.
 * @warning En la practica se permite HTTP SIN TLS exclusivamente en LAN.
 * Para despliegue real usar HTTPS con certificado CA verificado y firmas.
 */
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "configuracion.h"
#include "red_espnow.h"
#include "ota_manager.h"

static const char *TAG = "OTA";

void ota_confirmar_arranque(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t estado;
    esp_err_t err = esp_ota_get_state_partition(running, &estado);
    if (err == ESP_OK && estado == ESP_OTA_IMG_PENDING_VERIFY) {
        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK)
            ESP_LOGW(TAG, "Firmware nuevo VALIDADO; rollback confirmado");
        else
            ESP_LOGE(TAG, "Error validando imagen: %s", esp_err_to_name(err));
    }
}

esp_err_t ota_ejecutar(void)
{
    if (!red_wifi_conectado()) {
        ESP_LOGW(TAG, "OTA requiere Wi-Fi asociado a red 2.4 GHz");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "Comenzando OTA desde PC edge (HTTP solo laboratorio)");
    ESP_LOGW(TAG, "URL: %s", IOT_URL_FIRMWARE);
    const esp_http_client_config_t http_cfg = {
        .url = IOT_URL_FIRMWARE,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };
    const esp_https_ota_config_t cfg = {
        .http_config = &http_cfg,
    };
    esp_err_t err = esp_https_ota(&cfg);
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "OTA COMPLETADA: reiniciando al firmware actualizado");
        esp_restart();
    }
    ESP_LOGE(TAG, "OTA FALLO: %s", esp_err_to_name(err));
    return err;
}