#pragma once
//
// Copy this file to config.h and fill in your values.
// config.h is gitignored to keep credentials out of version control.
//

// =============================================================================
// WiFi Configuration
// =============================================================================
#define WIFI_SSID           "your_wifi_ssid"
#define WIFI_PASSWORD       "your_wifi_password"
#define WIFI_MAX_RETRY      5

// =============================================================================
// MQTT Configuration
// =============================================================================
#define MQTT_BROKER_URI     "mqtt://192.168.1.100:1883"
// Optional broker credentials (leave empty string "" if not required)
#define MQTT_USERNAME       ""
#define MQTT_PASSWORD       ""
#define MQTT_CLIENT_ID      "esp32c3-power-switch"
#define MQTT_QOS            0

// Topics to subscribe to (add/remove as needed)
#define MQTT_TOPICS         { "home/power/switch" }
#define MQTT_TOPIC_COUNT    1

// =============================================================================
// Hardware Pins
// =============================================================================
#define LED_GPIO            8

// I2C / OLED display (SSD1306, 72x40, address 0x3C)
#define I2C_PORT            I2C_NUM_0
#define I2C_SDA_GPIO        5
#define I2C_SCL_GPIO        6
#define I2C_FREQ_HZ         400000
#define OLED_I2C_ADDR       0x3C

// UART output (USB Serial/JTAG on GPIO18/GPIO19)
// Configured via sdkconfig.defaults — ESP_LOG goes through USB Serial/JTAG
