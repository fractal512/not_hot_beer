# 🍻 NOT_HOT_BEER

🍻 NOT_HOT_BEER project is an ESP32-based temperature controller designed for automatic refrigerator management.

The system measures the internal temperature using a **DS18B20** sensor, displays system information on an **OLED 128x64 display**, and controls two refrigerator relays through an **HL-52S dual relay module**.

Additional features include a buzzer, status indicator relay, built-in ESP32 status LED, Wi-Fi connectivity, and a web interface.

---

## Features

- 🌡️ Internal temperature measurement using **DS18B20**
- 🎛️ Adjustable target temperature (`T set`)
- ❄️ Automatic refrigerator control with hysteresis
- 💡 Indicator relay control
- 🔊 Audible feedback (buzzer) on button press
- 🖥️ OLED 128x64 display
- 💡 ESP32 built-in LED mirrors refrigerator status
- 📶 Wi-Fi connectivity
- 🌐 Embedded web server
- 💤 Automatic OLED sleep mode after inactivity
- 🛑 Emergency STOP button

---

## System Operation

Pressing the **START** button enables the refrigerator control system.

The refrigerators remain active until the internal temperature reaches the configured target temperature:

```text
T inside <= T set
        ↓
Refrigerators OFF
```

After that, they remain off until the temperature rises by 2°C:

```text
T inside >= T set + 2°C
        ↓
Refrigerators ON
```

This hysteresis prevents rapid relay switching and reduces compressor wear.

### Example

```text
T set = 5°C
```

Behavior:

```text
T inside > 7°C  → Refrigerators ON

T inside <= 5°C → Refrigerators OFF
```

---

## Buttons

| GPIO | Button | Function |
|------|---------|----------|
| GPIO 25 | DOWN | Decrease target temperature |
| GPIO 26 | UP | Increase target temperature |
| GPIO 33 | START | Start refrigerator control |
| GPIO 32 | STOP | Stop refrigerators and indicator relay |

Buttons are connected between the GPIO pin and **GND** and use the ESP32 internal pull-up resistors (`INPUT_PULLUP`).

Each button press generates a short buzzer beep.

---

## OLED Display

The OLED display shows:

- ESP32 IP address
- Internal temperature (`T inside`)
- Target temperature (`T set`)
- Refrigerator status
- System status

Example:

```text
IP: 192.168.1.105
----------------
T inside:  5.2 C
T set:     5.0 C
Fridges:   ON
System:    START
```

### Two-Color OLED

For standard 0.96" dual-color OLED displays:

- Upper section (yellow): IP address
- Lower section (blue): System information

---

## OLED Power Saving

If no buttons are pressed for **5 minutes**, the OLED enters power-saving mode.

The first button press:

- wakes up the display
- does not execute the button function

The next button press works normally.

The controller, sensors, relays, and web server continue operating while the OLED is sleeping.

---

## Indicator Relay

The single-channel relay controls a visual indicator.

Relay logic:

```text
HIGH → ON
LOW  → OFF
```

---

## HL-52S Relay Module

The refrigerator relays use active-low logic:

```text
FRIDGE_RELAY_ON  = LOW
FRIDGE_RELAY_OFF = HIGH
```

Both channels are used simultaneously to control refrigerator equipment.

---

## ESP32 Built-in LED

The built-in LED on **GPIO2** mirrors refrigerator status:

```text
LED ON  → Refrigerators running
LED OFF → Refrigerators stopped
```

This allows quick status verification without looking at the display.

---

## Web Interface

The ESP32 connects to Wi-Fi and starts a built-in web server.

The IP address is displayed on both:

- OLED display
- Serial Monitor

Example:

```text
http://192.168.1.105
```

The web page displays:

- IP address
- Internal temperature
- Target temperature
- Refrigerator status
- System status
- Indicator relay status
- OLED status

The page automatically refreshes every **10 seconds**.

---

## Wi-Fi Configuration

Create the file:

```text
include/secrets.h
```

Contents:

```cpp
#pragma once

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

Include it in the project:

```cpp
#include "secrets.h"
```

Do not upload this file to GitHub.

Add it to `.gitignore`:

```gitignore
include/secrets.h
```

You may also create:

```text
include/secrets.h.example
```

```cpp
#pragma once

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

---

## Hardware Connections

| Device | ESP32 GPIO |
|----------|-----------|
| DS18B20 DATA | GPIO 5 |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| DOWN Button | GPIO 25 |
| UP Button | GPIO 26 |
| STOP Button | GPIO 32 |
| START Button | GPIO 33 |
| Indicator Relay | GPIO 18 |
| HL-52S IN1 | GPIO 19 |
| HL-52S IN2 | GPIO 23 |
| Buzzer | GPIO 27 |
| Built-in LED | GPIO 2 |

### DS18B20

Use a **4.7 kΩ pull-up resistor** between:

```text
DATA ↔ 4.7kΩ ↔ VCC
```

---

## Development Environment

- ESP32 DevKit V1
- VS Code
- PlatformIO
- Arduino Framework

Required libraries:

- U8g2
- DallasTemperature
- OneWire
- WiFi
- WebServer

---

## Installation

1. Clone the repository.
2. Install PlatformIO.
3. Create `include/secrets.h`.
4. Configure Wi-Fi credentials.
5. Connect the ESP32 board.
6. Build the project.
7. Upload the firmware.
8. Open Serial Monitor at **115200 baud**.
9. Find the assigned IP address.
10. Open the IP address in a web browser.

---

## Safety Notice

This project may control equipment connected to **230V AC mains power**.

⚠️ **Mains voltage is dangerous and can cause serious injury or death.**

Always keep the low-voltage ESP32 circuitry electrically isolated from high-voltage AC wiring.

When testing, verify the control logic first without connecting mains-powered loads.

---

## Project Status

Development and testing in progress.

The main goal of the project is reliable temperature control of refrigeration equipment using ESP32, DS18B20, OLED display, and a web-based monitoring interface.
