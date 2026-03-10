#include "mqtt_manager.h"
#include "display_state.h"
#include "led.h"
#include "config.h"

#include <string.h>

#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "mqtt";
static uint32_t s_msg_count = 0;

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
        s_msg_count = 0;
        display_set_mqtt("MQTT:DC");
        display_set_topic("");
        display_set_msg("");
        display_refresh();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribe ack, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
    {
        // Use %.*s to print directly from the MQTT buffer — no size limit, no copy needed
        ESP_LOGI(TAG, "topic='%.*s'  payload='%.*s'",
                 event->topic_len, event->topic,
                 event->data_len,  event->data);

        char count_str[16];
        snprintf(count_str, sizeof(count_str), "Msgs:%" PRIu32, ++s_msg_count);
        display_set_topic(count_str);
        display_set_msg("");
        display_refresh();
        led_blink_once();
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) 
        {
            ESP_LOGE(TAG, "  TCP errno=%d",
                     event->error_handle->esp_transport_sock_errno);
        }
        display_set_mqtt("MQTT:ERR");
        display_refresh();
        break;

    default:
        break;
    }
}

void mqtt_manager_init(void)
{
    esp_mqtt_client_config_t cfg = 
    {
        .broker.address.uri = MQTT_BROKER_URI,
        .buffer.size        = MQTT_BUFFER_SIZE,
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
