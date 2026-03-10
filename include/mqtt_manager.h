#pragma once

/**
 * Initialise the MQTT client, connect to the configured broker,
 * and subscribe to all topics defined in config.h.
 */
void mqtt_manager_init(void);
