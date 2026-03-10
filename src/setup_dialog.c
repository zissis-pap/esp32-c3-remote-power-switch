#include "setup_dialog.h"
#include "runtime_config.h"

#include "hal/usb_serial_jtag_ll.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define COUNTDOWN_S      10
#define FIELD_TIMEOUT_S  30

static const char *TAG = "setup";

// =============================================================================
// Output
//   esp_rom_printf writes to the USB JTAG TX FIFO (GPIO18/19) and flushes
//   only on '\n'. usb_serial_jtag_ll_txfifo_flush() forces an immediate flush
//   after every call so prompts appear before the user starts typing.
//   No driver is involved — LL access is direct and conflict-free.
// =============================================================================

static void dlg_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    esp_rom_printf("%s", buf);
    usb_serial_jtag_ll_txfifo_flush();
}

// =============================================================================
// Input
//   Poll the RX FIFO directly via LL (GPIO18/19, same pins).
//   No driver needed — direct FIFO access, 10 ms sleep between polls.
// =============================================================================

static int read_char(int timeout_ms)
{
    for (int elapsed = 0; elapsed < timeout_ms; elapsed += 10) {
        if (usb_serial_jtag_ll_rxfifo_data_available()) {
            uint8_t c = 0;
            usb_serial_jtag_ll_read_rxfifo(&c, 1);
            return c;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return -1;
}

// =============================================================================
// Read a line with echo and backspace. Press Enter with no input → keep def.
// =============================================================================

static void read_line(char *buf, size_t maxlen, const char *def)
{
    size_t i = 0;

    while (i < maxlen - 1) {
        int c = read_char(FIELD_TIMEOUT_S * 1000);

        if (c < 0 || c == '\r' || c == '\n') {
            dlg_printf("\r\n");
            break;
        }
        if ((c == 127 || c == '\b') && i > 0) {
            i--;
            dlg_printf("\b \b");
            continue;
        }
        if (c < 32) continue;

        buf[i++] = (char)c;
        dlg_printf("%c", (char)c);  /* echo */
    }

    buf[i] = '\0';

    if (i == 0 && def != NULL)
        snprintf(buf, maxlen, "%s", def);
}

// =============================================================================
// Dialog helpers
// =============================================================================

static void print_current(void)
{
    dlg_printf("  WiFi SSID     : %s\r\n", g_cfg.wifi_ssid);
    dlg_printf("  WiFi Password : %s\r\n", g_cfg.wifi_password);
    dlg_printf("  MQTT Broker   : %s\r\n", g_cfg.mqtt_broker_uri);
    dlg_printf("  MQTT Username : %s\r\n", g_cfg.mqtt_username);
    dlg_printf("  MQTT Password : %s\r\n", g_cfg.mqtt_password);
    dlg_printf("  MQTT Topic    : %s\r\n", g_cfg.mqtt_topic);
}

static void prompt_field(const char *label, char *field, size_t field_size)
{
    char tmp[64];
    dlg_printf("  %s [%s]: ", label, field);
    read_line(tmp, sizeof(tmp), field);
    snprintf(field, field_size, "%s", tmp);
}

static void edit_all(void)
{
    dlg_printf("\r\nPress Enter to keep each value, or type a new one:\r\n\r\n");
    prompt_field("WiFi SSID    ", g_cfg.wifi_ssid,       sizeof(g_cfg.wifi_ssid));
    prompt_field("WiFi Password", g_cfg.wifi_password,   sizeof(g_cfg.wifi_password));
    prompt_field("MQTT Broker  ", g_cfg.mqtt_broker_uri, sizeof(g_cfg.mqtt_broker_uri));
    prompt_field("MQTT Username", g_cfg.mqtt_username,   sizeof(g_cfg.mqtt_username));
    prompt_field("MQTT Password", g_cfg.mqtt_password,   sizeof(g_cfg.mqtt_password));
    prompt_field("MQTT Topic   ", g_cfg.mqtt_topic,      sizeof(g_cfg.mqtt_topic));
}

// =============================================================================
// Entry point
// =============================================================================

void setup_dialog_run(void)
{
    /* Flush any spurious bytes already in the RX FIFO before starting. */
    while (usb_serial_jtag_ll_rxfifo_data_available()) {
        uint8_t discard;
        usb_serial_jtag_ll_read_rxfifo(&discard, 1);
    }

    dlg_printf("\r\n===========================\r\n");
    dlg_printf("  ESP32-C3 Power Switch\r\n");
    dlg_printf("       Boot Setup\r\n");
    dlg_printf("===========================\r\n");
    dlg_printf("\r\nCurrent settings:\r\n");
    print_current();
    dlg_printf("\r\n");

    bool edited = false;

    for (int t = COUNTDOWN_S; t > 0; t--) {
        dlg_printf("\rPress any key to edit, or wait %2ds for defaults...\n", t);
        int c = read_char(1000);
        if (c >= 0) {
            edit_all();
            edited = true;
            break;
        }
    }

    dlg_printf("\r\n");

    if (edited) {
        dlg_printf("Updated settings:\r\n");
        print_current();
        dlg_printf("\r\n");
    }

    dlg_printf("Starting...\r\n\r\n");

    ESP_LOGI(TAG, "SSID=%s  Broker=%s  User=%s  Topic=%s",
             g_cfg.wifi_ssid, g_cfg.mqtt_broker_uri,
             g_cfg.mqtt_username, g_cfg.mqtt_topic);
}
