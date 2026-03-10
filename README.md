# ESP32-C3 Remote Power Switch

An ESP32-C3 firmware that connects to a WiFi network, subscribes to MQTT topics, controls two power switch outputs, and displays live status on an SSD1306 OLED.

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-5.x-E7352C?logo=espressif&logoColor=white)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-6.x-F5822A?logo=platformio&logoColor=white)](https://platformio.org/)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-10.x-38A1CE?logoColor=white)](https://www.freertos.org/)
[![MQTT](https://img.shields.io/badge/MQTT-3.1.1-660066?logo=mqtt&logoColor=white)](https://mqtt.org/)
[![C](https://img.shields.io/badge/C-99-00599C?logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C99)

---

## Features

- WiFi station mode with configurable auto-reconnect
- MQTT subscriber — multiple topics, QoS 0/1
- **Two GPIO power switch outputs** (GPIO3, GPIO4) controlled via MQTT JSON messages
- SSD1306 OLED (72×40) shows live WiFi, IP, MQTT status, message counters, and switch states
- LED blinks on boot and on every received message
- All events logged in colour over USB Serial/JTAG (GPIO18/GPIO19)

---

## Hardware

| Peripheral | Pin(s) |
|---|---|
| LED | GPIO8 |
| Power switch 1 | GPIO3 |
| Power switch 2 | GPIO4 |
| I2C SDA (OLED) | GPIO5 |
| I2C SCL (OLED) | GPIO6 |
| OLED address | 0x3C |
| USB Serial/JTAG | GPIO18 (D−), GPIO19 (D+) |

Display: SSD1306-compatible, 72×40 px, I²C

---

## Project Structure

```
esp32-c3-remote-power-switch/
│
├── include/
│   ├── config_template.h   # Template — copy to config.h and fill credentials
│   ├── config.h            # (gitignored) WiFi/MQTT credentials and pin config
│   ├── ssd1306.h           # SSD1306 driver public API
│   ├── display_state.h     # Thread-safe 5-line display state manager
│   ├── led.h               # LED init and blink helpers
│   ├── power_switch.h      # Power switch GPIO control API
│   ├── wifi_manager.h      # WiFi station mode and reconnect logic
│   └── mqtt_manager.h      # MQTT client, subscriptions and message handling
│
├── src/
│   ├── main.c              # Boot sequence and app_main
│   ├── ssd1306.c           # SSD1306 I2C driver + 5×8 ASCII font
│   ├── display_state.c     # Display state storage and OLED refresh
│   ├── led.c               # GPIO output for LED
│   ├── power_switch.c      # GPIO3/GPIO4 init and MQTT-driven control
│   ├── wifi_manager.c      # WiFi init, event handler, event group
│   └── mqtt_manager.c      # MQTT init, event handler, topic subscription
│
├── sdkconfig.defaults       # ESP-IDF Kconfig defaults (MQTT stack size, etc.)
├── sdkconfig.esp32-c3-devkitc-02  # Board-specific Kconfig (tracked)
└── platformio.ini           # PlatformIO build and monitor configuration
```

---

## Display Layout

```
┌────────────────────┐
│ WiFi:OK            │  line 0 — WiFi status (init / conn... / rtyN / OK / FAIL)
│ 192.168.1.42       │  line 1 — IP address
│ MQTT:OK            │  line 2 — MQTT status (--- / OK / DC / ERR)
│ M:42 E:3           │  line 3 — message and error counters
│ P1:ON P2:OFF       │  line 4 — power switch states
└────────────────────┘
```

---

## Power Switch Control

GPIO3 and GPIO4 are initialised **HIGH** at boot (switches off).

Send a JSON message to the subscribed MQTT topic:

```json
{"switch": "1", "status": "ON"}
```

| Field | Values |
|---|---|
| `switch` | `"1"` → GPIO3 &nbsp;&nbsp; `"2"` → GPIO4 |
| `status` | `"ON"` → GPIO driven **LOW** &nbsp;&nbsp; `"OFF"` → GPIO driven **HIGH** |

Examples:

```json
{"switch": "1", "status": "ON"}   // GPIO3 LOW  — P1 on
{"switch": "1", "status": "OFF"}  // GPIO3 HIGH — P1 off
{"switch": "2", "status": "ON"}   // GPIO4 LOW  — P2 on
{"switch": "2", "status": "OFF"}  // GPIO4 HIGH — P2 off
```

> **Note:** JSON requires double quotes. Single-quoted payloads will be rejected.

The OLED line 4 updates immediately to reflect the new state.

Full JSON payloads are also recursively parsed and logged to UART:

```
I mqtt: ── MQTT message ──────────────────────
I mqtt:   topic:   switch
I mqtt:     switch           1
I mqtt:     status           ON
I mqtt: ──────────────────────────────────────
I pwr:  P1 (GPIO3) -> ON (LOW)
```

---

## Getting Started

### 1. Configure credentials

```sh
cp include/config_template.h include/config.h
```

Edit `include/config.h`:

```c
#define WIFI_SSID            "your_ssid"
#define WIFI_PASSWORD        "your_password"
#define MQTT_BROKER_URI      "mqtt://192.168.1.100:1883"
#define MQTT_TOPICS          { "switch" }
#define MQTT_TOPIC_COUNT     1
#define POWER_SWITCH_1_GPIO  3
#define POWER_SWITCH_2_GPIO  4
```

### 2. Build and flash

```sh
pio run -t upload
```

### 3. Monitor

```sh
pio device monitor
```

---

## MQTT Message Buffer

The default receive buffer is **32768 bytes** per message, configured in `config.h`:

```c
#define MQTT_BUFFER_SIZE  32768
```

Reduce this value if memory is tight, or increase it for very large JSON payloads.

---

## Notes

- **Column offset**: if OLED content appears horizontally shifted, adjust `SSD1306_COL_OFFSET` in `include/ssd1306.h` (default `28` — correct for most 72×40 modules).
- **USB console**: `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y` is already set in the sdkconfig, so ESP-IDF colour logs appear in `pio device monitor` with no extra configuration.
- **Active-low outputs**: ON drives the GPIO LOW; OFF drives it HIGH. Wire your relay or load accordingly.
