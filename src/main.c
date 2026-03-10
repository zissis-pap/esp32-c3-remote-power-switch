#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2c.h"

#include "config.h"
#include "ssd1306.h"
#include "display_state.h"
#include "led.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

static const char *TAG = "main";

static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_GPIO,
        .scl_io_num       = I2C_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C ready (SDA=GPIO%d SCL=GPIO%d)", I2C_SDA_GPIO, I2C_SCL_GPIO);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C3 MQTT Subscriber booting ===");

    // NVS — required by WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();
    i2c_init();

    ssd1306_init(I2C_PORT, OLED_I2C_ADDR);
    display_state_init();
    display_refresh();

    led_blink_once();   // signal boot

    wifi_manager_init();

    EventBits_t bits = xEventGroupWaitBits(wifi_manager_get_event_group(),
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi up — starting MQTT");
        mqtt_manager_init();
    } else {
        ESP_LOGE(TAG, "WiFi failed — halting");
        display_set_mqtt("MQTT:skip");
        display_refresh();
        while (true) {
            led_set(true);  vTaskDelay(pdMS_TO_TICKS(200));
            led_set(false); vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    ESP_LOGI(TAG, "Running");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Heartbeat — free heap: %" PRIu32 " bytes",
                 esp_get_free_heap_size());
    }
}
