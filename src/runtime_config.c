#include "runtime_config.h"
#include "config.h"

#include <stdio.h>

runtime_config_t g_cfg;

void runtime_config_load_defaults(void)
{
    const char *topics[] = MQTT_TOPICS;

    snprintf(g_cfg.wifi_ssid,       CFG_SSID_MAX,  "%s", WIFI_SSID);
    snprintf(g_cfg.wifi_password,   CFG_PASS_MAX,  "%s", WIFI_PASSWORD);
    snprintf(g_cfg.mqtt_broker_uri, CFG_URI_MAX,   "%s", MQTT_BROKER_URI);
    snprintf(g_cfg.mqtt_username,   CFG_USER_MAX,  "%s", MQTT_USERNAME);
    snprintf(g_cfg.mqtt_password,   CFG_PASS_MAX,  "%s", MQTT_PASSWORD);
    snprintf(g_cfg.mqtt_topic,      CFG_TOPIC_MAX, "%s", topics[0]);
}
