# MushroomController

ESP32 firmware that reads temperature/humidity from an SHT31 sensor,
displays it on a 128x64 SSD1306 OLED, and publishes it over MQTT for a
backend to collect.

**Firmware version:** 1.3.0

Sketch folder: `MushroomController/` (Arduino requires folder name = `.ino` name).

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
| `MushroomController.ino` | Entry point — `setup()` / `loop()` |
| `Config.h` | Pins, targets, device identity, timing (no hardcoded WiFi/MQTT host) |
| `Topics.h` | MQTT topic strings |
| `ProvisioningStore.h` / `.cpp` | Preferences: WiFi + MQTT host/port/user/pass/tls/mode |
| `GrowNetworkManager.h` / `.cpp` | SoftAP two-step portal (WiFi → MQTT), tests, reconnect |
| `MqttClient.h` / `.cpp` | Broker connect (plain or TLS), LWT/birth, publish JSON |
| `Sensor` / `Display` / `DeviceInfo` / `Icons` | Sensor, OLED, payload metadata |

## SoftAP provisioning (WiFi + MQTT)

WiFi and MQTT broker settings are **not** compiled in. They are stored in
flash (`Preferences`) via a SoftAP portal.

### Why the dashboard suggests an IP

While your phone is only on SoftAP (`192.168.4.1`), it cannot reach
MycoMonitor on the LAN. Copy the suggested MQTT host from the dashboard
**before** joining SoftAP, or use the portal’s suggestion after step 1
(when the ESP has joined farm WiFi in AP+STA mode).

### Flow

1. Join SoftAP `GrowOS-Setup-XXXX` → open `http://192.168.4.1`.
2. **Step 1 — WiFi:** pick SSID, password, Connect. SoftAP stays up; device joins STA.
3. **Step 2 — MQTT:** Local or Cloud mode, host/port/(user/pass).
   - `GET /suggest` — gateway IP + optional backend `/health` probe
   - **Test MQTT** — real CONNECT to the broker
   - **Test backend** (local) — `GET http://{host}:4000/health`
4. **Save & Finish** — requires a successful MQTT test (re-runs on save if needed), then reboot.

### Local vs cloud

| Mode | Port default | TLS | Auth |
|---|---|---|---|
| Local | 1883 | no | optional |
| Cloud | 8883 | yes (`WiFiClientSecure`, `setInsecure()` for now) | username required |

CA certificate pinning for cloud TLS is a follow-up hardening step.

### Preferences keys

`wifi_ssid`, `wifi_password`, `mqtt_host`, `mqtt_port`, `mqtt_user`,
`mqtt_pass`, `mqtt_tls`, `mqtt_mode`.

Missing WiFi **or** MQTT host → Setup Mode (resume at the right step).

## MQTT topics

| Topic | Published | Contents |
|---|---|---|
| `farm/mushroom/unit1/sensors` | Every `SENSOR_READ_INTERVAL_MS` | Sensor + targets + device info |
| `farm/mushroom/unit1/status` | Connect (retained) + LWT | `{"online":true}` / `{"online":false}` |

## Setup (Arduino IDE)

1. Open `MushroomController/MushroomController.ino`.
2. Board: **ESP32 Dev Module**.
3. In `Config.h`, set `DEVICE_ID` / `DEVICE_NAME` / targets as needed (not broker IP).
4. Upload.
5. Complete SoftAP steps above (or use MycoMonitor **ESP device setup** panel for the suggested host).
6. Serial 115200: SoftAP instructions, `MQTT connected to host:port`, `Published to MQTT`.

## Required libraries

- Adafruit GFX, SSD1306, SHT31
- PubSubClient, ArduinoJson

`Preferences`, `WebServer`, `DNSServer`, `HTTPClient`, `WiFiClientSecure` ship with the ESP32 core.

## Not built yet

- MQTT commands / relays
- Runtime target changes over MQTT
- OTA
- Cloud TLS CA pinning

See `wificonfig.md` for architecture notes.
