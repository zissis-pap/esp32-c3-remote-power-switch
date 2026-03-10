#include "led.h"
#include "config.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    led_set(false);
}

void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

void led_blink_once(void)
{
    led_set(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set(false);
}
