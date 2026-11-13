/**
 * @file pantalla.c
 * @brief Interfaz visual sin LVGL: textos y estados en pantalla GC9A01.
 * @details Utiliza el componente oficial esp_lcd_gc9a01. Pines Waveshare
 * Touch: MOSI11 SCLK10 CS9 DC8 RST14 BL2. Pantalla de 240x240.
 */
#include "configuracion.h"
#include "pantalla.h"

#if IOT_USAR_PANTALLA

#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_gc9a01.h"
#include "esp_log.h"
#include "esp_check.h"
#include "fuente5x7.h"

#define RES_X 240
#define RES_Y 240
#define RGB565_BLACK 0x0000
#define RGB565_WHITE 0xFFFF
#define RGB565_CYAN  0x07FF
#define RGB565_GREEN 0x07E0
#define RGB565_RED   0xF800
#define RGB565_AMBER 0xFD20

static const char *TAG = "PANTALLA";
static esp_lcd_panel_handle_t s_panel = NULL;
/* Buffer completo estable para DMA, evita reutilizar stripes en envio asincrono. */
static uint16_t s_fondo[RES_X * RES_Y] __attribute__((aligned(4)));

/** @brief Pinta un rectangulo sin salir de los limites del panel. */
static void rect(int x, int y, int w, int h, uint16_t color)
{
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= RES_Y) continue;
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= RES_X) continue;
            s_fondo[yy * RES_X + xx] = color;
        }
    }
}

/** @brief Busca el patron 5x7 de un caracter ASCII. */
static const uint8_t *glifo(char c)
{
    for (size_t i = 0; i < GLOSARIO_N; i++)
        if (GLOSARIO[i].simbolo == c) return GLOSARIO[i].col;
    return GLOSARIO[GLOSARIO_N - 1].col; /* espacio */
}

/** @brief Dibuja texto en pixeles RGB565 con factor de escala. */
static void texto(const char *s, int y, int escala, uint16_t color)
{
    if (!s) return;
    const int n = (int)strlen(s);
    const int ancho = n * 6 * escala;
    const int inicio = (RES_X - ancho) / 2;
    for (int k = 0; k < n; k++) {
        const uint8_t *g = glifo(s[k]);
        for (int x = 0; x < 5; x++) {
            for (int yy = 0; yy < 7; yy++) {
                if (g[x] & (1 << yy))
                    rect(inicio + k * 6 * escala + x * escala,
                         y + yy * escala, escala, escala, color);
            }
        }
    }
}

esp_err_t pantalla_iniciar(void)
{
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << 2,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl), TAG, "backlight GPIO2");
    gpio_set_level(GPIO_NUM_2, 0);

    spi_bus_config_t bus = {
        .sclk_io_num = 10,
        .mosi_io_num = 11,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = RES_X * RES_Y * 2 + 8,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO),
                        TAG, "SPI2 LCD");

    esp_lcd_panel_io_spi_config_t io = {
        .dc_gpio_num = 8,
        .cs_gpio_num = 9,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &io_handle), TAG, "panel IO");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = 14,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(
        io_handle, &panel_cfg, &s_panel), TAG, "panel GC9A01");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset LCD");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init LCD");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "on LCD");
    gpio_set_level(GPIO_NUM_2, 1);
    ESP_LOGI(TAG, "LCD GC9A01 iniciado (240x240)");
    pantalla_mostrar("INICIO", "IOT IA");
    return ESP_OK;
}

void pantalla_mostrar(const char *estado, const char *detalle)
{
    if (!s_panel) return;
    uint16_t acento = RGB565_CYAN;
    if (estado && (strstr(estado, "ON") || strstr(estado, "LISTO")))
        acento = RGB565_GREEN;
    else if (estado && (strstr(estado, "OFF") || strstr(estado, "ERROR")))
        acento = RGB565_RED;
    else if (estado && strstr(estado, "OTA"))
        acento = RGB565_AMBER;

    for (int i = 0; i < RES_X * RES_Y; i++) s_fondo[i] = RGB565_BLACK;
    rect(27, 63, 186, 3, acento);
    rect(28, 166, 184, 2, acento);
    texto("IOT IA", 34, 3, RGB565_CYAN);
    if (estado) texto(estado, 97, 3, acento);
    if (detalle) texto(detalle, 190, 2, RGB565_WHITE);
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, 0, 0, RES_X, RES_Y, s_fondo);
    if (err != ESP_OK) ESP_LOGW(TAG, "LCD draw: %s", esp_err_to_name(err));
}

#else

esp_err_t pantalla_iniciar(void) { return ESP_OK; }
void pantalla_mostrar(const char *estado, const char *detalle)
{
    (void)estado;
    (void)detalle;
}

#endif