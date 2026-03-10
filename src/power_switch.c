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

// Apply a single channel state and drive the GPIO.
static bool apply(int ch, const char *status_str)
{
    bool on = (strcmp(status_str, "ON") == 0);
    if (ch == 1) {
        s_p1_on = on;
        gpio_set_level(POWER_SWITCH_1_GPIO, on ? 0 : 1);
        ESP_LOGI(TAG, "P1 (GPIO%d) -> %s", POWER_SWITCH_1_GPIO, on ? "ON (LOW)" : "OFF (HIGH)");
        return true;
    } else if (ch == 2) {
        s_p2_on = on;
        gpio_set_level(POWER_SWITCH_2_GPIO, on ? 0 : 1);
        ESP_LOGI(TAG, "P2 (GPIO%d) -> %s", POWER_SWITCH_2_GPIO, on ? "ON (LOW)" : "OFF (HIGH)");
        return true;
    }
    ESP_LOGW(TAG, "Unknown channel: %d", ch);
    return false;
}

void power_switch_handle(const cJSON *root)
{
    bool updated = false;

    // Explicit two-key format: {"switch1": "ON", "switch2": "OFF"}
    const cJSON *sw1 = cJSON_GetObjectItem(root, "switch1");
    const cJSON *sw2 = cJSON_GetObjectItem(root, "switch2");
    if (cJSON_IsString(sw1)) updated |= apply(1, sw1->valuestring);
    if (cJSON_IsString(sw2)) updated |= apply(2, sw2->valuestring);

    // Walk the raw linked list to catch every "switch"/"status" pair, including
    // duplicate keys (e.g. {"switch":"1","status":"ON","switch":"2","status":"ON"}).
    // cJSON_GetObjectItem only returns the first match, so we must walk manually.
    const cJSON *node = root->child;
    while (node != NULL) {
        if (cJSON_IsString(node) && node->string &&
            strcmp(node->string, "switch") == 0) {
            const cJSON *next = node->next;
            if (next && cJSON_IsString(next) && next->string &&
                strcmp(next->string, "status") == 0) {
                updated |= apply(atoi(node->valuestring), next->valuestring);
                node = next->next;
                continue;
            }
        }
        node = node->next;
    }

    if (updated) update_display();
}
