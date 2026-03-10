#include "display_state.h"
#include "ssd1306.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define DISP_COLS   (SSD1306_CHARS_PER_LINE + 1)   // 12 visible chars + NUL

static SemaphoreHandle_t s_mutex;

static char s_wifi[DISP_COLS];
static char s_ip[DISP_COLS];
static char s_mqtt[DISP_COLS];
static char s_topic[DISP_COLS];
static char s_msg[DISP_COLS];

void display_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    snprintf(s_wifi,  DISP_COLS, "WiFi:init");
    snprintf(s_ip,    DISP_COLS, "IP:---");
    snprintf(s_mqtt,  DISP_COLS, "MQTT:---");
    s_topic[0] = '\0';
    s_msg[0]   = '\0';
}

void display_set_wifi(const char *status)  { snprintf(s_wifi,  DISP_COLS, "%s", status); }
void display_set_ip(const char *ip)        { snprintf(s_ip,    DISP_COLS, "%s", ip);     }
void display_set_mqtt(const char *status)  { snprintf(s_mqtt,  DISP_COLS, "%s", status); }
void display_set_topic(const char *topic)  { snprintf(s_topic, DISP_COLS, "%s", topic);  }
void display_set_msg(const char *msg)      { snprintf(s_msg,   DISP_COLS, "%s", msg);    }

void display_refresh(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ssd1306_clear();
    ssd1306_write_string(0, 0, s_wifi);
    ssd1306_write_string(0, 1, s_ip);
    ssd1306_write_string(0, 2, s_mqtt);
    ssd1306_write_string(0, 3, s_topic);
    ssd1306_write_string(0, 4, s_msg);
    ssd1306_update();
    xSemaphoreGive(s_mutex);
}
