/**
 * @file main.c
 * @brief Receptor ESP-NOW con LED, consulta de estado, ACK y descubrimiento.
 * @details ESP-IDF v6.1. Adaptacion segura en longitud de mensajes del
 * ejemplo esp_now_receptor entregado por el ingeniero.
 * @warning Comunicacion SIN cifrado. Demostracion solamente en laboratorio.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "protocolo.h"
#include "configuracion_receptor.h"

#define RX_QUEUE_LENGTH 10

static const char *TAG = "ESP_NOW_RECEPTOR";
static const uint8_t BROADCAST[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
static QueueHandle_t cola;
static int led_encendido = 0;

typedef struct {
    iot_trama_t trama;
    uint8_t origen[6];
} evento_t;

/** @brief Agrega un par para poder responder con mensajes unicast. */
static esp_err_t agregar_peer(const uint8_t *mac)
{
    if (esp_now_is_peer_exist(mac)) return ESP_OK;
    esp_now_peer_info_t p = {0};
    memcpy(p.peer_addr, mac, 6);
    p.channel = 0;
    p.ifidx = WIFI_IF_STA;
    p.encrypt = false;
    return esp_now_add_peer(&p);
}

/** @brief Callback liviano: pasa el paquete a una cola para procesarlo. */
static void recibir_cb(const esp_now_recv_info_t *info,
                       const uint8_t *data, int len)
{
    if (!info || !data || len != (int)sizeof(iot_trama_t)) return;
    evento_t ev;
    memcpy(&ev.trama, data, sizeof(ev.trama));
    if (!iot_trama_valida(&ev.trama)) return;
    memcpy(ev.origen, info->src_addr, 6);
    if (cola) xQueueSend(cola, &ev, 0);
}

/** @brief Activa o desactiva la salida LED fisica. */
static void actualizar_led(int on)
{
    led_encendido = on ? 1 : 0;
    const int nivel = IOT_LED_ACTIVO_ALTO ? led_encendido : !led_encendido;
    gpio_set_level((gpio_num_t)IOT_LED_GPIO, nivel);
}

/** @brief Ejecuta comando recibido y devuelve ACK de aplicacion. */
static void atender(const evento_t *ev)
{
    if (ev->trama.tipo != IOT_T_COMANDO) return;
    esp_err_t err = agregar_peer(ev->origen);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo agregar par: %s", esp_err_to_name(err));
        return;
    }

    switch (ev->trama.comando) {
        case IOT_CMD_ON:
            actualizar_led(1);
            ESP_LOGW(TAG, "ORDEN IA: LED ENCENDIDO");
            break;
        case IOT_CMD_OFF:
            actualizar_led(0);
            ESP_LOGW(TAG, "ORDEN IA: LED APAGADO");
            break;
        case IOT_CMD_ESTADO:
            ESP_LOGW(TAG, "CONSULTA: LED=%s", led_encendido ? "ON" : "OFF");
            break;
        default:
            ESP_LOGW(TAG, "Comando desconocido: %u", ev->trama.comando);
            return;
    }
    const iot_trama_t ack = iot_trama(IOT_T_ACK, ev->trama.comando,
                                      led_encendido, ev->trama.secuencia);
    err = esp_now_send(ev->origen, (const uint8_t *)&ack, sizeof(ack));
    if (err != ESP_OK) ESP_LOGE(TAG, "ACK fallo: %s", esp_err_to_name(err));
}

/** @brief Punto de entrada del receptor ESP32 DevKit. */
void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);

    gpio_reset_pin((gpio_num_t)IOT_LED_GPIO);
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)IOT_LED_GPIO, GPIO_MODE_OUTPUT));
    actualizar_led(0);

    cola = xQueueCreate(RX_QUEUE_LENGTH, sizeof(evento_t));
    if (!cola) { ESP_LOGE(TAG, "Sin RAM para cola"); return; }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(IOT_ESPNOW_CANAL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(recibir_cb));
    ESP_ERROR_CHECK(agregar_peer(BROADCAST));

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGW(TAG, "RECEPTOR LISTO MAC=" MACSTR " CANAL=%u LED_GPIO=%d",
             MAC2STR(mac), IOT_ESPNOW_CANAL, IOT_LED_GPIO);
    ESP_LOGW(TAG, "Envia HELLO cada 3 segundos hasta encontrar al nodo Waveshare");

    int64_t ultimo_hello = 0;
    while (true) {
        evento_t ev;
        if (xQueueReceive(cola, &ev, pdMS_TO_TICKS(100)) == pdTRUE) atender(&ev);
        const int64_t ahora = esp_timer_get_time();
        if ((ahora - ultimo_hello) > 3000000LL) {
            const iot_trama_t hello = iot_trama(IOT_T_HELLO, IOT_CMD_NINGUNO,
                                               led_encendido, 0);
            esp_now_send(BROADCAST, (const uint8_t *)&hello, sizeof(hello));
            ultimo_hello = ahora;
        }
    }
}