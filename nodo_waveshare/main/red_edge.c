/**
 * @file red_edge.c
 * @brief Envio de telemetria JSON al PC edge sobre Wi-Fi en LAN.
 * @note HTTP de laboratorio SIN TLS. No utilizar en Internet sin HTTPS.
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "configuracion.h"
#include "red_espnow.h"
#include "red_edge.h"

static const char *TAG = "EDGE";
static float s_http_rtt_ms = -1.0f;

float edge_ultimo_rtt_ms(void) { return s_http_rtt_ms; }

esp_err_t edge_publicar(float ax, float ay, float az,
                       const char *gesto, float confianza,
                       int led, float rtt_espnow_ms)
{
    if (!red_wifi_conectado()) return ESP_ERR_INVALID_STATE;
    char json[320];
    int n = snprintf(json, sizeof(json),
        "{\"nodo\":\"WAVESHARE-S3\",\"version\":\"%s\","
        "\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,"
        "\"gesto\":\"%s\",\"confianza\":%.3f,"
        "\"led\":%d,\"rtt_espnow_ms\":%.2f}",
        IOT_VERSION_FIRMWARE, ax, ay, az,
        gesto ? gesto : "DESCONOCIDO", confianza, led, rtt_espnow_ms);
    if (n < 0 || n >= (int)sizeof(json)) return ESP_ERR_INVALID_SIZE;

    esp_http_client_config_t cfg = {
        .url = IOT_URL_TELEMETRIA,
        .timeout_ms = 4000,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, n);

    const int64_t inicio = esp_timer_get_time();
    esp_err_t err = esp_http_client_perform(client);
    s_http_rtt_ms = (esp_timer_get_time() - inicio) / 1000.0f;
    if (err == ESP_OK) {
        int codigo = esp_http_client_get_status_code(client);
        if (codigo >= 200 && codigo < 300) {
            ESP_LOGI(TAG, "POST OK HTTP=%d RTT=%.1f ms", codigo, s_http_rtt_ms);
        } else {
            ESP_LOGW(TAG, "POST HTTP=%d RTT=%.1f ms", codigo, s_http_rtt_ms);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "No responde PC edge: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
}