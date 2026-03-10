#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "mqtt_client.h"

#include "config.h"
#include "ssd1306.h"

static const char *TAG = "mqtt_sub";

// =============================================================================
// WiFi
// =============================================================================
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t   s_wifi_event_group;
static int                  s_retry_count = 0;

// =============================================================================
// Display state (one string per display line, 12 chars + NUL)
// =============================================================================
#define DISP_COLS   (SSD1306_CHARS_PER_LINE + 1)   // 13

static SemaphoreHandle_t s_disp_mutex;

static char s_disp_wifi[DISP_COLS]  = "WiFi:init";
static char s_disp_ip[DISP_COLS]    = "IP:---";
static char s_disp_mqtt[DISP_COLS]  = "MQTT:---";
static char s_disp_topic[DISP_COLS] = "";
static char s_disp_msg[DISP_COLS]   = "";

static void display_refresh(void)
{
    xSemaphoreTake(s_disp_mutex, portMAX_DELAY);
    ssd1306_clear();
    ssd1306_write_string(0, 0, s_disp_wifi);
    ssd1306_write_string(0, 1, s_disp_ip);
    ssd1306_write_string(0, 2, s_disp_mqtt);
    ssd1306_write_string(0, 3, s_disp_topic);
    ssd1306_write_string(0, 4, s_disp_msg);
    ssd1306_update();
    xSemaphoreGive(s_disp_mutex);
}

// =============================================================================
// LED helpers
// =============================================================================
static void led_set(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
}

static void led_blink_once(void)
{
    led_set(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set(false);
}

// =============================================================================
// WiFi event handler
// =============================================================================
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        snprintf(s_disp_wifi, DISP_COLS, "WiFi:conn...");
        display_refresh();
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            s_retry_count++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d", s_retry_count, WIFI_MAX_RETRY);
            snprintf(s_disp_wifi, DISP_COLS, "WiFi:rty%d", s_retry_count);
            display_refresh();
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "WiFi connection failed after %d attempts", WIFI_MAX_RETRY);
            snprintf(s_disp_wifi, DISP_COLS, "WiFi:FAIL");
            snprintf(s_disp_ip,   DISP_COLS, "IP:---");
            display_refresh();
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        snprintf(s_disp_wifi, DISP_COLS, "WiFi:OK");
        {
            char ip_buf[16];
            snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(&event->ip_info.ip));
            strncpy(s_disp_ip, ip_buf, DISP_COLS - 1);
            s_disp_ip[DISP_COLS - 1] = '\0';
        }
        display_refresh();
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// =============================================================================
// MQTT event handler
// =============================================================================
static void mqtt_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event  = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to broker");
        snprintf(s_disp_mqtt, DISP_COLS, "MQTT:OK");
        display_refresh();
        led_blink_once();

        // Subscribe to all configured topics
        {
            const char *topics[] = MQTT_TOPICS;
            for (int i = 0; i < MQTT_TOPIC_COUNT; i++) {
                int msg_id = esp_mqtt_client_subscribe(client, topics[i], MQTT_QOS);
                ESP_LOGI(TAG, "Subscribed to '%s', msg_id=%d", topics[i], msg_id);
            }
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        snprintf(s_disp_mqtt,  DISP_COLS, "MQTT:DC");
        s_disp_topic[0] = '\0';
        s_disp_msg[0]   = '\0';
        display_refresh();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        // Null-terminate topic and payload (they are not NUL-terminated by the library)
        char topic[64]   = {0};
        char payload[64] = {0};
        int tlen = event->topic_len   < (int)sizeof(topic) - 1
                        ? event->topic_len   : (int)sizeof(topic) - 1;
        int plen = event->data_len    < (int)sizeof(payload) - 1
                        ? event->data_len    : (int)sizeof(payload) - 1;
        memcpy(topic,   event->topic, tlen);
        memcpy(payload, event->data,  plen);

        ESP_LOGI(TAG, "MQTT data  topic='%s'  payload='%s'", topic, payload);

        snprintf(s_disp_topic, DISP_COLS, "T:%.10s", topic);
        snprintf(s_disp_msg,   DISP_COLS, "M:%.10s", payload);
        display_refresh();
        led_blink_once();
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "  TCP transport error, errno=%d",
                     event->error_handle->esp_transport_sock_errno);
        }
        snprintf(s_disp_mqtt, DISP_COLS, "MQTT:ERR");
        display_refresh();
        break;

    default:
        break;
    }
}

// =============================================================================
// Initialisers
// =============================================================================
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
    ESP_LOGI(TAG, "I2C initialised (SDA=GPIO%d, SCL=GPIO%d)", I2C_SDA_GPIO, I2C_SCL_GPIO);
}

static void led_init(void)
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
    ESP_LOGI(TAG, "LED initialised on GPIO%d", LED_GPIO);
}

static void wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done, waiting for connection...");
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials = {
            .client_id = MQTT_CLIENT_ID,
            .username  = (strlen(MQTT_USERNAME) ? MQTT_USERNAME : NULL),
            .authentication.password = (strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : NULL),
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
    ESP_LOGI(TAG, "MQTT client started, broker=%s", MQTT_BROKER_URI);
}

// =============================================================================
// app_main
// =============================================================================
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C3 MQTT Subscriber booting ===");

    // NVS — required by WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Peripherals
    led_init();
    i2c_init();

    // Display startup screen
    s_disp_mutex = xSemaphoreCreateMutex();
    ssd1306_init(I2C_PORT, OLED_I2C_ADDR);
    snprintf(s_disp_wifi, DISP_COLS, "WiFi:init");
    display_refresh();

    // Blink LED once to signal boot
    led_blink_once();

    // WiFi — blocks until connected or failed
    wifi_init();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected, starting MQTT");
        mqtt_init();
    } else {
        ESP_LOGE(TAG, "WiFi failed — MQTT will not start. Halting.");
        snprintf(s_disp_mqtt, DISP_COLS, "MQTT:skip");
        display_refresh();
        // Blink LED rapidly to signal error
        while (true) {
            led_set(true);  vTaskDelay(pdMS_TO_TICKS(200));
            led_set(false); vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    // Main task has nothing more to do — FreeRTOS tasks run the show
    ESP_LOGI(TAG, "Setup complete, entering idle loop");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Heartbeat — heap free: %" PRIu32 " bytes",
                 esp_get_free_heap_size());
    }
}
