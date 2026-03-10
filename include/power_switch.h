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
 * Supported formats:
 *
 *   Single channel:  {"switch": "1", "status": "ON"}
 *   Both channels:   {"switch1": "ON", "switch2": "OFF"}
 *
 * ON  → GPIO driven LOW
 * OFF → GPIO driven HIGH
 *
 * Updates the display msg line; does NOT call display_refresh().
 */
void power_switch_handle(const cJSON *root);
