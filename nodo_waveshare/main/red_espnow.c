/**
 * @file red_espnow.c
 * @brief Descubrimiento y comandos ESP-NOW con ACK y RTT.
 * @details Adaptado del ejemplo transmisor/receptor del ingeniero,
 * utilizando callback esp_now_recv_info_t y esp_now_send_info_t de IDF 6.1.
 * @warning Tramas sin cifrado/autenticacion, solo prototipo en laboratorio.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "configuracion.h"
#include "protocolo.h"
#include "red_espnow.h"

#define RX_QUEUE_LEN 12
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "ESP_NOW_S3";
static const uint8_t BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct {
    iot_trama_t trama;
    uint8_t origen[6];
} evento_now_t;

static QueueHandle_t s_cola;
static EventGroupHandle_t s_wifi_ev;
static uint8_t s_peer_mac[6];
static bool s_enlace_listo = false;
static bool s_wifi_ok = false;
static uint32_t s_secuencia = 0;
static uint32_t s_enviado_seq = 0;
static int64_t s_enviado_us = 0;
static float s_rtt_ms = -1.0f;
static int s_led_remoto = -1;

/** @brief Gestiona conexion a AP cuando se proporcionan credenciales. */
static void wifi_evento(void *arg, esp_event_base_t base,
                        int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_ok = false;
        xEventGroupClearBits(s_wifi_ev, WIFI_CONNECTED_BIT);
        if (IOT_WIFI_SSID[0]) esp_wifi_connect();
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_wifi_ok = true;
        xEventGroupSetBits(s_wifi_ev, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi asociado: Edge disponible");
    }
}

/** @brief Recibe mensajes ligeros y los copia a una cola FreeRTOS. */
static void cb_recibir(const esp_now_recv_info_t *info,
                       const uint8_t *data, int len)
{
    if (!info || !data || len != (int)sizeof(iot_trama_t)) return;
    evento_now_t e;
    memcpy(&e.trama, data, sizeof(e.trama));
    if (!iot_trama_valida(&e.trama)) return;
    memcpy(e.origen, info->src_addr, 6);
    if (s_cola) xQueueSend(s_cola, &e, 0);
}

/** @brief Notifica exito de entrega a nivel MAC, NO a nivel aplicacion. */
static void cb_enviar(const esp_now_send_info_t *info,
                      esp_now_send_status_t estado)
{
    (void)info;
    if (estado != ESP_NOW_SEND_SUCCESS)
        ESP_LOGW(TAG, "Error de envio a nivel MAC");
}

/** @brief Agrega un peer unicast descubierto; evita MAC fijada en codigo. */
static esp_err_t agregar_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) return ESP_OK;
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0; /* Usa canal actual Wi-Fi */
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false; /* Demo: sin intercambio de claves */
    return esp_now_add_peer(&peer);
}

uint8_t red_canal_actual(void)
{
    uint8_t primario = 0;
    wifi_second_chan_t secundario = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primario, &secundario) != ESP_OK) return 0;
    return primario;
}

esp_err_t red_iniciar(void)
{
    s_cola = xQueueCreate(RX_QUEUE_LEN, sizeof(evento_now_t));
    s_wifi_ev = xEventGroupCreate();
    if (!s_cola || !s_wifi_ev) return ESP_ERR_NO_MEM;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED, wifi_evento, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP, wifi_evento, NULL));

    if (IOT_WIFI_SSID[0]) {
        wifi_config_t conf = {0};
        strncpy((char *)conf.sta.ssid, IOT_WIFI_SSID, sizeof(conf.sta.ssid) - 1);
        strncpy((char *)conf.sta.password, IOT_WIFI_PASS,
                sizeof(conf.sta.password) - 1);
        conf.sta.threshold.authmode = WIFI_AUTH_OPEN;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &conf));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    if (IOT_WIFI_SSID[0]) {
        ESP_LOGI(TAG, "Conectando a Wi-Fi 2.4 GHz...");
        ESP_ERROR_CHECK(esp_wifi_connect());
        EventBits_t bits = xEventGroupWaitBits(s_wifi_ev,
            WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(12000));
        if (!(bits & WIFI_CONNECTED_BIT))
            ESP_LOGW(TAG, "Sin AP, ESP-NOW trabajara con el canal Wi-Fi actual");
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_channel(IOT_ESPNOW_CANAL, WIFI_SECOND_CHAN_NONE));
    }

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(cb_recibir));
    ESP_ERROR_CHECK(esp_now_register_send_cb(cb_enviar));
    ESP_ERROR_CHECK(agregar_peer(BROADCAST));

    uint8_t mi_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mi_mac));
    ESP_LOGI(TAG, "MAC Waveshare: " MACSTR, MAC2STR(mi_mac));
    ESP_LOGI(TAG, "CANAL ACTUAL: %u; configurar IGUAL en receptor", red_canal_actual());
    ESP_LOGI(TAG, "Esperando HELLO del receptor ESP-NOW...");
    return ESP_OK;
}

void red_procesar(void)
{
    evento_now_t e;
    while (s_cola && xQueueReceive(s_cola, &e, 0) == pdTRUE) {
        if (e.trama.tipo == IOT_T_HELLO) {
            if (!s_enlace_listo || memcmp(s_peer_mac, e.origen, 6) != 0) {
                if (agregar_peer(e.origen) == ESP_OK) {
                    memcpy(s_peer_mac, e.origen, 6);
                    s_enlace_listo = true;
                    ESP_LOGW(TAG, "RECEPTOR DESCUBIERTO " MACSTR, MAC2STR(e.origen));
                }
            }
        } else if (e.trama.tipo == IOT_T_ACK) {
            if (s_enlace_listo && memcmp(s_peer_mac, e.origen, 6) == 0) {
                s_led_remoto = e.trama.estado_led;
                if (e.trama.secuencia == s_enviado_seq && s_enviado_us > 0) {
                    s_rtt_ms = (esp_timer_get_time() - s_enviado_us) / 1000.0f;
                    ESP_LOGW(TAG, "ACK seq=%lu LED=%d RTT=%.2f ms",
                        (unsigned long)e.trama.secuencia, s_led_remoto, s_rtt_ms);
                    s_enviado_us = 0;
                }
            }
        }
    }
}

esp_err_t red_enviar(uint8_t comando)
{
    if (!s_enlace_listo) {
        ESP_LOGW(TAG, "No se ha descubierto receptor. Encender segundo ESP32");
        return ESP_ERR_INVALID_STATE;
    }
    const uint32_t seq = ++s_secuencia;
    iot_trama_t t = iot_trama(IOT_T_COMANDO, comando, 0, seq);
    esp_err_t err = esp_now_send(s_peer_mac, (const uint8_t *)&t, sizeof(t));
    if (err == ESP_OK) {
        s_enviado_seq = seq;
        s_enviado_us = esp_timer_get_time();
        ESP_LOGI(TAG, "CMD=%u seq=%lu enviado", comando, (unsigned long)seq);
    } else {
        ESP_LOGE(TAG, "Fallo esp_now_send: %s", esp_err_to_name(err));
    }
    return err;
}

bool red_receptor_descubierto(void) { return s_enlace_listo; }
bool red_wifi_conectado(void) { return s_wifi_ok; }
float red_ultimo_rtt_ms(void) { return s_rtt_ms; }
int red_estado_remoto(void) { return s_led_remoto; }