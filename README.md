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
- MQTT subscriber with configurable broker, credentials, and topic
- **Two GPIO power switch outputs** (GPIO3, GPIO4) controlled via MQTT JSON messages
- **Interactive boot-time setup dialog** over USB Serial/JTAG — configure WiFi and MQTT without reflashing
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
│   ├── runtime_config.h    # Runtime config struct (g_cfg) and defaults loader
│   ├── setup_dialog.h      # Boot-time interactive setup dialog
│   ├── ssd1306.h           # SSD1306 driver public API
│   ├── display_state.h     # Thread-safe 5-line display state manager
│   ├── led.h               # LED init and blink helpers
│   ├── power_switch.h      # Power switch GPIO control API
│   ├── wifi_manager.h      # WiFi station mode and reconnect logic
│   └── mqtt_manager.h      # MQTT client, subscriptions and message handling
│
├── src/
│   ├── main.c              # Boot sequence and app_main
│   ├── runtime_config.c    # g_cfg initialisation from config.h defaults
│   ├── setup_dialog.c      # Interactive setup over USB Serial/JTAG
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

## Boot Setup Dialog

On every boot a setup dialog is shown over USB Serial/JTAG (`pio device monitor`). It displays the current settings and waits **10 seconds** for input before proceeding with defaults.

```
===========================
  ESP32-C3 Power Switch
       Boot Setup
===========================

Current settings:
  WiFi SSID     : MyNetwork
  WiFi Password : secret
  MQTT Broker   : mqtt://192.168.1.100:1883
  MQTT Username : user
  MQTT Password : pass
  MQTT Topic    : switch

Press any key to edit, or wait 10s for defaults...
```

Press **any key** to enter edit mode. Each field shows its current value in brackets — press **Enter** to keep it or type a new value:

```
Press Enter to keep each value, or type a new one:

  WiFi SSID     [MyNetwork]:
  WiFi Password [secret]:
  MQTT Broker   [mqtt://192.168.1.100:1883]: mqtt://10.0.0.5:1883
  MQTT Username [user]:
  MQTT Password [pass]: newpass
  MQTT Topic    [switch]:
```

If no key is pressed within 10 seconds the dialog exits silently and the compile-time defaults from `config.h` are used. Settings entered in the dialog are held in RAM for the current session only and are not persisted to flash.

---

## Power Switch Control

GPIO3 and GPIO4 are initialised **HIGH** at boot (switches off).

Send a JSON message to the subscribed MQTT topic. Two formats are supported:

### Single channel

```json
{"switch": "1", "status": "ON"}
```

| Field | Values |
|---|---|
| `switch` | `"1"` → GPIO3 &nbsp;&nbsp; `"2"` → GPIO4 |
| `status` | `"ON"` → GPIO driven **LOW** &nbsp;&nbsp; `"OFF"` → GPIO driven **HIGH** |

### Both channels in one message

Two formats are accepted:

**Repeated `switch`/`status` pairs** (duplicate keys — cJSON linked list is walked manually so both pairs are processed):

```json
{"switch": "1", "status": "ON", "switch": "2", "status": "ON"}
```

**Explicit `switch1`/`switch2` keys** (standard valid JSON):

```json
{"switch1": "ON", "switch2": "OFF"}
```

Either or both channel keys may be present; only the channels included in the message are updated.

### Examples

```json
{"switch": "1", "status": "ON"}    // GPIO3 LOW  — P1 on
{"switch": "1", "status": "OFF"}   // GPIO3 HIGH — P1 off
{"switch": "2", "status": "ON"}    // GPIO4 LOW  — P2 on
{"switch": "2", "status": "OFF"}   // GPIO4 HIGH — P2 off

{"switch": "1", "status": "ON", "switch": "2", "status": "ON"}   // both on
{"switch": "1", "status": "OFF", "switch": "2", "status": "OFF"} // both off
{"switch": "1", "status": "ON", "switch": "2", "status": "OFF"}  // P1 on, P2 off

{"switch1": "ON",  "switch2": "ON"}   // both on
{"switch1": "OFF", "switch2": "OFF"}  // both off
{"switch1": "ON",  "switch2": "OFF"}  // P1 on, P2 off
```

> **Note:** JSON requires double quotes. Single-quoted payloads will be rejected.

The OLED line 4 updates immediately to reflect the new state.

---

## Getting Started

### 1. Configure credentials

```sh
cp include/config_template.h include/config.h
```

Edit `include/config.h` with your compile-time defaults:

```c
#define WIFI_SSID            "your_ssid"
#define WIFI_PASSWORD        "your_password"
#define MQTT_BROKER_URI      "mqtt://192.168.1.100:1883"
#define MQTT_USERNAME        ""
#define MQTT_PASSWORD        ""
#define MQTT_TOPICS          { "switch" }
#define MQTT_TOPIC_COUNT     1
#define POWER_SWITCH_1_GPIO  3
#define POWER_SWITCH_2_GPIO  4
```

These values are the defaults shown in the boot dialog and used when no input is given.

### 2. Build and flash

```sh
pio run -t upload
```

### 3. Open the monitor before or immediately after reset

```sh
pio device monitor
```

The setup dialog appears for 10 seconds. Press any key to edit settings for this session.

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
- **Session-only settings**: values entered in the boot dialog are stored in RAM and reset on power cycle. To make them permanent, update `config.h` and reflash.
