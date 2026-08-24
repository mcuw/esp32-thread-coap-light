#include "led_strip.h"
#include "esp_log.h"
#include "ws2812_control.h"

static const char *TAG = "ws2812";
#define LED_GPIO 8

static led_strip_handle_t s_strip;
static bool s_on = false;
static uint8_t s_r = 255, s_g = 255, s_b = 255;  // letzte gesetzte Farbe

void ws2812_control_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip));
    led_strip_clear(s_strip);
    ESP_LOGI(TAG, "WS2812 is ready on GPIO%d", LED_GPIO);
}

void ws2812_control_set(bool on, uint8_t r, uint8_t g, uint8_t b)
{
    s_on = on;
    if (on) {
        s_r = r; s_g = g; s_b = b;
        led_strip_set_pixel(s_strip, 0, r, g, b);
        led_strip_refresh(s_strip);
    } else {
        led_strip_clear(s_strip);
    }
    ESP_LOGI(TAG, "LED %s (R=%d G=%d B=%d)", on ? "On" : "Off", s_r, s_g, s_b);
}

void ws2812_control_get(bool *on, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *on = s_on; *r = s_r; *g = s_g; *b = s_b;
}