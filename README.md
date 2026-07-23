# MushroomController

ESP32 firmware that reads temperature/humidity from an SHT31 sensor,
displays it on a 128x64 SSD1306 OLED, and publishes it over MQTT for a
backend to collect.

```
┌────────────────────────────┐
│ T 31.3°C        +3.3°C     │
│                            │
│ H 79.5%      -10.5% RH     │
│ WiFi:OK  MQTT:OK           │
└────────────────────────────┘
```

## File structure

| File | Purpose |
|---|---|
| `MushroomController.ino` | Entry point — `setup()` / `loop()`, wires everything together |
| `Config.h` | Pins, I2C addresses, target temp/humidity, device identity, MQTT settings, WiFi timing / SoftAP prefix |
| `Topics.h` | All MQTT topic strings in one place |
| `Sensor.h` / `Sensor.cpp` | Wraps the SHT31 sensor, returns a `SensorReading` struct that can serialize itself (`fill()`) |
| `Display.h` / `Display.cpp` | Wraps the SSD1306, renders the T/H + delta UI and WiFi status (OK / Connecting / Setup Mode) |
| `Icons.h` | 16x16 thermometer and water-drop bitmaps |
| `GrowNetworkManager.h` / `GrowNetworkManager.cpp` | Preferences-backed WiFi, SoftAP setup portal, non-blocking reconnect, fail-into-setup (named to avoid ESP32 Core 3.x `NetworkManager`) |
| `MqttClient.h` / `MqttClient.cpp` | Connects to the broker (with last-will/birth messages) and publishes each reading as JSON |
| `DeviceInfo.h` / `DeviceInfo.cpp` | Adds device identity/health fields (firmware, IP, WiFi signal) to any JSON doc |
| `wificonfig.md` | Design notes for the WiFi setup architecture |

## MQTT topics

Built at runtime from `MQTT_TOPIC_PREFIX` + `DEVICE_ID` (see `Topics.h` / `Config.h`):

| Topic | Published | Contents |
|---|---|---|
| `farm/mushroom/unit1/sensors` | Every `SENSOR_READ_INTERVAL_MS` | Sensor reading + targets + device info |
| `farm/mushroom/unit1/status` | On connect (retained), and automatically by the broker on unexpected disconnect (Last Will) | `{"online":true}` / `{"online":false}` |

`commands` and `events` suffixes are reserved in `Topics.h` for later
(see "Not built yet" below) so the topic hierarchy won't need to change.

## Sensor payload

```json
{
  "device_id": "unit1",
  "device_name": "Grow Room 1",
  "device_type": "controller",
  "firmware": "1.2.0",
  "ip": "192.168.1.45",
  "wifi_rssi": -58,

  "temperature": 31.3,
  "humidity": 79.5,
  "sensor_ok": true,

  "target_temperature": 28,
  "target_humidity": 90,

  "uptime_ms": 123456
}
```

**Note on targets vs. deltas:** the device sends the raw reading *and* the
target that was active at publish time — not a precomputed delta. If you
change `TARGET_TEMPERATURE`/`TARGET_HUMIDITY` later (e.g. moving from
incubation to fruiting), old rows in your database still make sense on
their own, and the backend can compute `temperature - target_temperature`
whenever it needs to. The OLED still shows the delta directly, since
that's the one place a precomputed number is actually useful (a human
glancing at a screen, not a database).

## Status topic (Last Will / Birth message)

On every successful connect, the device publishes a **retained**
`{"online":true}` to its status topic — so a dashboard that connects
*after* the device already published still sees the current state
immediately.

It also registers a **Last Will**: if the device loses power or drops off
the network ungracefully, the broker publishes `{"online":false}` on its
behalf, without the device having to do anything. This is how the
dashboard shows 🔴 offline instead of just going stale with old numbers.

## Wiring (ESP32 Dev Module)

| Signal | ESP32 Pin |
|---|---|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| OLED + SHT31 VCC | 3.3V |
| OLED + SHT31 GND | GND |

Both the OLED (`0x3C`) and SHT31 (`0x44`) sit on the same I2C bus.
GPIO 21/22 are the default I2C pins on the ESP32 Dev Module, so no
`Wire.begin()` argument changes are needed.

## Required libraries (Arduino Library Manager)

- Adafruit GFX Library
- Adafruit SSD1306
- Adafruit SHT31
- PubSubClient (MQTT)
- ArduinoJson

`Preferences`, `WebServer`, and `DNSServer` ship with the ESP32 Arduino core — no extra Library Manager installs.

## Setup

1. Open `MushroomController.ino` in the Arduino IDE (keep all files in this
   same folder — the sketch name must match the folder name if you rename it).
2. Board: **ESP32 Dev Module**.
3. In `Config.h`, set:
   - `MQTT_BROKER` (IP or hostname of your Mosquitto broker / cloud broker)
   - `DEVICE_ID` / `DEVICE_NAME` — unique per unit if you run more than one grow room
   - `TARGET_TEMPERATURE` / `TARGET_HUMIDITY` for your current growth stage
4. Upload.
5. **WiFi (first boot / no saved credentials):**
   - Device opens SoftAP `GrowOS-Setup-XXXX` (`XXXX` = last 4 hex digits of the MAC).
   - Join that network on your phone/laptop, then open `http://192.168.4.1`.
   - Pick (or type) your home SSID, enter the password, Save & Connect.
   - Credentials are stored in flash via Preferences (`wifi_ssid` / `wifi_password`).
   - On success the device restarts and joins STA + MQTT.
6. Watch the Serial Monitor (115200 baud) for SoftAP SSID / `WiFi connected` /
   `Published to MQTT`. OLED bottom line: `Setup Mode`, `Connecting...`, or `WiFi:OK MQTT:OK`.

If STA connect fails repeatedly (`WIFI_MAX_FAILED_ATTEMPTS`, default 5), the
device automatically returns to SoftAP Setup Mode so you can reconfigure
without a USB cable.

## Multiple units

Each grow room can run its own ESP32 with a unique `DEVICE_ID` (`unit1`,
`unit2`, ...) publishing to its own topic. Subscribe the backend to a
wildcard (`farm/mushroom/+/sensors` and `farm/mushroom/+/status`) to
receive all units at once.

## Not built yet (intentionally deferred)

These came up as good next steps but aren't implemented — building them
before you have a working end-to-end pipeline (device → broker → backend
→ dashboard) would be premature:

- **Commands** (`.../commands` topic) — subscribing so the backend can
  turn on a fan/humidifier relay from the dashboard. Needs an MQTT
  *subscribe* callback and relay wiring, which is its own chunk of work.
- **Runtime configuration** — changing `TARGET_TEMPERATURE` etc. over MQTT
  instead of recompiling, with values persisted to flash. Useful once
  you're managing several units and don't want to re-flash each one by hand.
- **OTA updates** — pushing new firmware without a USB cable. Worth doing
  once you have more than a couple of units deployed and re-flashing by
  hand gets tedious.
