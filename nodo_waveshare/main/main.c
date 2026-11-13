/**
 * @file main.c
 * @brief Nodo IoT Waveshare: IA LDA, gestos, ESP-NOW, EDGE y OTA.
 * @author Proyecto academico - Ingenieria Electronica
 * @date 2026-11-13
 *
 * @details
 * - Sensor IMU QMI8658 por I2C (GPIO6/7)
 * - Clasificador LDA entrenado con 915 muestras reales
 * - 4 comandos HMI por inclinacion
 * - ESP-NOW: LED remoto y RTT mediante ACK de aplicacion
 * - HTTP hacia computador edge: telemetria JSON y RTT
 * - Actualizaciones OTA por red local con particiones A/B y rollback
 * - Pantalla LCD redonda GC9A01 con resultados de inferencia
 *
 * @warning HTTP y ESP-NOW de demostracion no usan cifrado; no desplegar
 * en redes publicas o sistemas que actuen sobre cargas peligrosas.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "configuracion.h"
#include "imu.h"
#include "modelo_ia.h"
#include "red_espnow.h"
#include "red_edge.h"
#include "ota_manager.h"
#include "pantalla.h"
#include "protocolo.h"

#define MUESTREO_MS 100
#define REPORTE_MS 1500
#define TELEMETRIA_MS 6000
#define CONFIRMACIONES 4
#define OTA_CONFIRMACIONES 35 /* ~3.5 segundos de gesto sostenido */

static const char *TAG = "IOT_INTEGRADO";

/** @brief Texto corto para mostrar una clase en LCD GC9A01. */
static const char *lcd_clase(ia_clase_t clase)
{
    switch (clase) {
        case IA_REPOSO: return "REPOSO";
        case IA_X_NEG: return "X NEG";
        case IA_X_POS: return "X POS";
        case IA_Y_NEG: return "Y NEG";
        case IA_Y_POS: return "Y POS";
        default: return "DESCON";
    }
}

/**
 * @brief Envia la orden reconocida a otro ESP32.
 * @param clase Clase confirmada por la IA.
 * @param confianza Probabilidad softmax relativa, no calibrada.
 */
static void ejecutar_accion(ia_clase_t clase, float confianza)
{
    esp_err_t err = ESP_OK;
    switch (clase) {
        case IA_X_POS:
            ESP_LOGW(TAG, "GESTO IA -> ENCENDER LED REMOTO (%.1f%%)", confianza * 100);
            pantalla_mostrar("LED ON", "ESP NOW");
            err = red_enviar(IOT_CMD_ON);
            break;
        case IA_X_NEG:
            ESP_LOGW(TAG, "GESTO IA -> APAGAR LED REMOTO (%.1f%%)", confianza * 100);
            pantalla_mostrar("LED OFF", "ESP NOW");
            err = red_enviar(IOT_CMD_OFF);
            break;
        case IA_Y_POS:
            ESP_LOGW(TAG, "GESTO IA -> CONSULTAR LED REMOTO (%.1f%%)", confianza * 100);
            pantalla_mostrar("ESTADO", "ESP NOW");
            err = red_enviar(IOT_CMD_ESTADO);
            break;
        default:
            return;
    }
    if (err != ESP_OK)
        ESP_LOGW(TAG, "Comando no entregado: %s", esp_err_to_name(err));
}

/** @brief Inicializa almacenamiento NVS necesario para Wi-Fi. */
static esp_err_t preparar_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

/**
 * @brief Punto de entrada de firmware integrado.
 *
 * La captura y clasificacion ocurren a 10 Hz; solo las lineas
 * informativas se muestran cada 1.5 s. Las acciones requieren 4
 * predicciones consecutivas, y regresar a REPOSO antes de otro gesto.
 * OTA requiere gesto Y_NEG sostenido ~3.5 s y enlace a Wi-Fi.
 */
void app_main(void)
{
    ESP_LOGW(TAG, "============================================");
    ESP_LOGW(TAG, "IoT ESP32-S3 v%s - IA + ESP-NOW + EDGE + OTA", IOT_VERSION_FIRMWARE);
    ESP_LOGW(TAG, "============================================");
    ESP_ERROR_CHECK(preparar_nvs());
    ESP_ERROR_CHECK(imu_iniciar());
    /* Se marca validacion solo si un sensor critico inicializo bien. */
    ota_confirmar_arranque();
    ESP_ERROR_CHECK(red_iniciar());
    esp_err_t lcd_err = pantalla_iniciar();
    if (lcd_err != ESP_OK) {
        ESP_LOGW(TAG, "LCD no disponible: %s (sistema sigue por serial)",
                 esp_err_to_name(lcd_err));
    } else {
        pantalla_mostrar("LISTO", "IA Y RED");
    }

    ESP_LOGW(TAG, "X_POS=ON; X_NEG=OFF; Y_POS=ESTADO; Y_NEG=OTA 3.5s");
    ESP_LOGW(TAG, "Para repetir un gesto, volver primero a REPOSO");
    ESP_LOGW(TAG, "IMPORTANTE: los nombres X/Y son etiquetas fisicas de captura");

    ia_clase_t anterior = IA_DESCONOCIDO;
    int consecutivas = 0;
    bool habilitado = true;
    TickType_t ultimo_informe = xTaskGetTickCount();
    TickType_t ultima_telemetria = xTaskGetTickCount();

    while (true) {
        red_procesar();
        float ax = 0, ay = 0, az = 0;
        esp_err_t err = imu_aceleraciones(&ax, &ay, &az);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C: %s", esp_err_to_name(err));
            anterior = IA_DESCONOCIDO;
            consecutivas = 0;
            vTaskDelay(pdMS_TO_TICKS(MUESTREO_MS));
            continue;
        }

        ia_resultado_t p = ia_predecir(ax, ay, az);
        TickType_t ahora = xTaskGetTickCount();
        if (ahora - ultimo_informe >= pdMS_TO_TICKS(REPORTE_MS)) {
            ESP_LOGI(TAG,
                "AX=%+.3fg AY=%+.3fg AZ=%+.3fg | IA=%s %.1f%% | PEER=%s CANAL=%u LED=%d",
                ax, ay, az, ia_nombre(p.clase), p.confianza * 100.0f,
                red_receptor_descubierto() ? "SI" : "NO",
                red_canal_actual(), red_estado_remoto());
            ultimo_informe = ahora;
        }

        if (ahora - ultima_telemetria >= pdMS_TO_TICKS(TELEMETRIA_MS)) {
            if (red_wifi_conectado()) {
                edge_publicar(ax, ay, az, ia_nombre(p.clase),
                    p.confianza, red_estado_remoto(), red_ultimo_rtt_ms());
            }
            ultima_telemetria = xTaskGetTickCount();
        }

        if (p.clase == IA_DESCONOCIDO) {
            consecutivas = 0;
            anterior = IA_DESCONOCIDO;
        } else {
            if (p.clase == anterior) {
                if (consecutivas < OTA_CONFIRMACIONES) consecutivas++;
            } else {
                anterior = p.clase;
                consecutivas = 1;
            }

            if (p.clase == IA_REPOSO && consecutivas >= CONFIRMACIONES) {
                if (!habilitado) ESP_LOGI(TAG, "Regreso a REPOSO, listo para otro gesto");
                habilitado = true;
            } else if (habilitado && p.clase != IA_REPOSO) {
                if (p.clase == IA_Y_NEG) {
                    if (consecutivas == CONFIRMACIONES) {
                        ESP_LOGW(TAG, "Gesto OTA detectado: mantener 3.5 segundos para confirmar");
                        pantalla_mostrar("OTA 3S", "MANTENER");
                    }
                    if (consecutivas >= OTA_CONFIRMACIONES) {
                        habilitado = false;
                        pantalla_mostrar("OTA", "DESCARGA");
                        err = ota_ejecutar();
                        if (err != ESP_OK) pantalla_mostrar("ERROR", "OTA FALLA");
                    }
                } else if (consecutivas >= CONFIRMACIONES) {
                    habilitado = false;
                    ejecutar_accion(p.clase, p.confianza);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(MUESTREO_MS));
    }
}