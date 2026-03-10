#include "mqtt_manager.h"
#include "display_state.h"
#include "led.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "mqtt";
static uint32_t s_msg_count = 0;
static uint32_t s_err_count = 0;

// =============================================================================
// Helpers
// =============================================================================

// Recursively log every node in a JSON tree, indented by depth
static void log_json_node(const cJSON *node, int depth)
{
    char indent[17];
    int spaces = depth * 2;
    if (spaces > (int)sizeof(indent) - 1) spaces = (int)sizeof(indent) - 1;
    memset(indent, ' ', spaces);
    indent[spaces] = '\0';

    while (node) {
        const char *key = node->string ? node->string : "-";
        if (cJSON_IsObject(node) || cJSON_IsArray(node)) {
            ESP_LOGI(TAG, "%s%s:", indent, key);
            log_json_node(node->child, depth + 1);
        } else if (cJSON_IsNumber(node)) {
            ESP_LOGI(TAG, "%s%-16s %.4g", indent, key, node->valuedouble);
        } else if (cJSON_IsString(node)) {
            ESP_LOGI(TAG, "%s%-16s %s",   indent, key, node->valuestring);
        } else if (cJSON_IsBool(node)) {
            ESP_LOGI(TAG, "%s%-16s %s",   indent, key, cJSON_IsTrue(node) ? "true" : "false");
        } else if (cJSON_IsNull(node)) {
            ESP_LOGI(TAG, "%s%-16s null", indent, key);
        }
        node = node->next;
    }
}

static void log_json_payload(const char *data, int dlen)
{
    char *buf = malloc(dlen + 1);
    if (!buf) { ESP_LOGW(TAG, "  (malloc failed)"); return; }
    memcpy(buf, data, dlen);
    buf[dlen] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) { ESP_LOGW(TAG, "  (invalid JSON)"); return; }

    log_json_node(root->child, 1);
    cJSON_Delete(root);
}

// Push updated counters to display line 3
static void display_update_counts(void)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "M:%" PRIu32 " E:%" PRIu32, s_msg_count, s_err_count);
    display_set_topic(buf);
}

// =============================================================================
// Event handler
// =============================================================================
static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    esp_mqtt_event_handle_t  event  = (esp_mqtt_event_handle_t)data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        ESP_LOGI(TAG, "Connected to broker");
        display_set_mqtt("MQTT:OK");
        display_refresh();
        led_blink_once();

        const char *topics[] = MQTT_TOPICS;
        for (int i = 0; i < MQTT_TOPIC_COUNT; i++)
        {
            int msg_id = esp_mqtt_client_subscribe(client, topics[i], MQTT_QOS);
            ESP_LOGI(TAG, "Subscribed '%s', msg_id=%d", topics[i], msg_id);
        }
        break;
    }

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected");
        display_set_mqtt("MQTT:DC");
        display_refresh();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribe ack, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "── MQTT message ──────────────────────");
        ESP_LOGI(TAG, "  topic:   %.*s", event->topic_len, event->topic);
        log_json_payload(event->data, event->data_len);
        ESP_LOGI(TAG, "──────────────────────────────────────");

        ++s_msg_count;
        display_update_counts();
        display_refresh();
        led_blink_once();
        break;
    }

    case MQTT_EVENT_ERROR:
    {
        ESP_LOGE(TAG, "Error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGE(TAG, "  TCP errno=%d",
                     event->error_handle->esp_transport_sock_errno);
        }
        ++s_err_count;
        display_set_mqtt("MQTT:ERR");
        display_update_counts();
        display_refresh();
        break;
    }

    default:
        break;
    }
}

// =============================================================================
// Init
// =============================================================================
void mqtt_manager_init(void)
{
    esp_mqtt_client_config_t cfg =
    {
        .broker.address.uri    = MQTT_BROKER_URI,
        .buffer.size           = MQTT_BUFFER_SIZE,
        .session.keepalive     = 60,
        .credentials = {
            .client_id               = MQTT_CLIENT_ID,
            .username                = (strlen(MQTT_USERNAME) ? MQTT_USERNAME : NULL),
            .authentication.password = (strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : NULL),
        },
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                                   event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
    ESP_LOGI(TAG, "Client started, broker=%s", MQTT_BROKER_URI);
}
