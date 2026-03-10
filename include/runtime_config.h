#pragma once

#define CFG_SSID_MAX    32
#define CFG_PASS_MAX    64
#define CFG_URI_MAX     64
#define CFG_USER_MAX    32
#define CFG_TOPIC_MAX   64

typedef struct {
    char wifi_ssid[CFG_SSID_MAX];
    char wifi_password[CFG_PASS_MAX];
    char mqtt_broker_uri[CFG_URI_MAX];
    char mqtt_username[CFG_USER_MAX];
    char mqtt_password[CFG_PASS_MAX];
    char mqtt_topic[CFG_TOPIC_MAX];
} runtime_config_t;

/** Global runtime configuration — populated by runtime_config_load_defaults()
 *  and optionally overridden by setup_dialog_run(). */
extern runtime_config_t g_cfg;

/** Fill g_cfg from the compile-time defaults in config.h. */
void runtime_config_load_defaults(void);
