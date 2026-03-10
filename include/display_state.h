#pragma once

/**
 * Initialise the display state manager.
 * Must be called after ssd1306_init().
 */
void display_state_init(void);

/** Update individual display lines. Changes are not visible until display_refresh() is called. */
void display_set_wifi(const char *status);
void display_set_ip(const char *ip);
void display_set_mqtt(const char *status);
void display_set_topic(const char *topic);
void display_set_msg(const char *msg);

/** Push all five lines to the OLED (thread-safe). */
void display_refresh(void);
