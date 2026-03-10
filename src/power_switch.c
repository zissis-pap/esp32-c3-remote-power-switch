#include "power_switch.h"
#include "display_state.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "pwr";

static bool s_p1_on = false;
static bool s_p2_on = false;

// Format: "P1:ON P2:ON " or "P1:OFFP2:OFF" etc.
// "P1:%-3sP2:%-3s" always produces exactly 12 chars — fits the 12-char display.
static void update_display(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "P1:%-3sP2:%-3s",
             s_p1_on ? "ON" : "OFF",
             s_p2_on ? "ON" : "OFF");
    display_set_msg(buf);
}

void power_switch_init(void)
{
    gpio_set_direction(POWER_SWITCH_1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(POWER_SWITCH_1_GPIO, 1);

    gpio_set_direction(POWER_SWITCH_2_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(POWER_SWITCH_2_GPIO, 1);

    ESP_LOGI(TAG, "GPIO%d (P1) and GPIO%d (P2) initialised HIGH",
             POWER_SWITCH_1_GPIO, POWER_SWITCH_2_GPIO);

    update_display();
}

void power_switch_handle(const cJSON *root)
{
    const cJSON *sw = cJSON_GetObjectItem(root, "switch");
    const cJSON *st = cJSON_GetObjectItem(root, "status");

    if (!cJSON_IsString(sw) || !cJSON_IsString(st)) return;

    int  ch = atoi(sw->valuestring);
    bool on = (strcmp(st->valuestring, "ON") == 0);

    if (ch == 1) {
        s_p1_on = on;
        gpio_set_level(POWER_SWITCH_1_GPIO, on ? 0 : 1);
        ESP_LOGI(TAG, "P1 (GPIO%d) -> %s", POWER_SWITCH_1_GPIO, on ? "ON (LOW)" : "OFF (HIGH)");
    } else if (ch == 2) {
        s_p2_on = on;
        gpio_set_level(POWER_SWITCH_2_GPIO, on ? 0 : 1);
        ESP_LOGI(TAG, "P2 (GPIO%d) -> %s", POWER_SWITCH_2_GPIO, on ? "ON (LOW)" : "OFF (HIGH)");
    } else {
        ESP_LOGW(TAG, "Unknown switch value: \"%s\"", sw->valuestring);
        return;
    }

    update_display();
}
