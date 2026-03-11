#pragma once

#include "cJSON.h"

/**
 * Configure GPIO3 and GPIO4 as outputs, both driven HIGH (switches off).
 * Also initialises the display line showing P1/P2 state.
 * Call once at boot before any MQTT messages arrive.
 */
void power_switch_init(void);

/**
 * Parse a JSON object and drive GPIO3/GPIO4 accordingly.
 *
 * Format:  {"switch": "1", "status": "ON"}
 *
 * To control both channels in one message, repeat the key pair:
 *   {"switch": "1", "status": "ON", "switch": "2", "status": "OFF"}
 *
 * ON  → GPIO driven LOW
 * OFF → GPIO driven HIGH
 *
 * Updates the display msg line; does NOT call display_refresh().
 */
void power_switch_handle(const cJSON *root);
