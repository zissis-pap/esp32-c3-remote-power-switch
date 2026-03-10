#include "wifi_manager.h"
#include "display_state.h"
#include "runtime_config.h"
#include "config.h"

#include <string.h>
#include <stdio.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "wifi";

static EventGroupHandle_t s_event_group;
static int                s_retry_count = 0;

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    char buf[16];

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA started, connecting...");
        display_set_wifi("WiFi:conn...");
        display_refresh();
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            s_retry_count++;
            ESP_LOGW(TAG, "Disconnected, retry %d/%d", s_retry_count, WIFI_MAX_RETRY);
            snprintf(buf, sizeof(buf), "WiFi:rty%d", s_retry_count);
            display_set_wifi(buf);
            display_refresh();
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Connection failed after %d attempts", WIFI_MAX_RETRY);
            display_set_wifi("WiFi:FAIL");
            display_set_ip("IP:---");
            display_refresh();
            xEventGroupSetBits(s_event_group, WIFI_FAIL_BIT);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        display_set_wifi("WiFi:OK");
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&event->ip_info.ip));
        display_set_ip(buf);
        display_refresh();
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_init(void)
{
    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = { 0 };
    snprintf((char *)wifi_cfg.sta.ssid,     sizeof(wifi_cfg.sta.ssid),     "%s", g_cfg.wifi_ssid);
    snprintf((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", g_cfg.wifi_password);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Init done, waiting for connection...");
}

EventGroupHandle_t wifi_manager_get_event_group(void)
{
    return s_event_group;
}
