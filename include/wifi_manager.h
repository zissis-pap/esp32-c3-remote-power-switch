#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

/**
 * Initialise WiFi in station mode and begin connecting.
 * Returns immediately — poll the event group to wait for the result.
 */
void wifi_manager_init(void);

/** Returns the event group set by the WiFi event handler. */
EventGroupHandle_t wifi_manager_get_event_group(void);
