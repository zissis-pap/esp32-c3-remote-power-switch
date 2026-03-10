#pragma once

#include "driver/i2c.h"
#include <stdint.h>

// Physical display dimensions
#define SSD1306_WIDTH       72
#define SSD1306_HEIGHT      40
#define SSD1306_PAGES       (SSD1306_HEIGHT / 8)   // 5 pages
// Column offset: 72-pixel wide display inside a 128-column SSD1306 controller.
// (128 - 72) / 2 = 28. Adjust if your display shows shifted content.
#define SSD1306_COL_OFFSET  28

// Max characters per line using 6-pixel wide font (5px glyph + 1px spacing)
#define SSD1306_CHARS_PER_LINE  (SSD1306_WIDTH / 6)   // 12

/**
 * @brief Initialise the SSD1306 display.
 *
 * The I2C master must be installed before calling this.
 *
 * @param port  I2C port number
 * @param addr  7-bit I2C address (0x3C or 0x3D)
 */
void ssd1306_init(i2c_port_t port, uint8_t addr);

/**
 * @brief Clear the internal framebuffer (call ssd1306_update to push to display).
 */
void ssd1306_clear(void);

/**
 * @brief Write a null-terminated ASCII string into the framebuffer.
 *
 * Characters are rendered using a 5x8 pixel font with 1 pixel horizontal spacing.
 * Text that exceeds the display width is clipped.
 *
 * @param col   Starting column (0 = left edge)
 * @param page  Starting page row (0–4 for 40-pixel tall display)
 * @param str   String to render
 */
void ssd1306_write_string(uint8_t col, uint8_t page, const char *str);

/**
 * @brief Push the framebuffer to the display over I2C.
 */
void ssd1306_update(void);
