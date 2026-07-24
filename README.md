# MushroomController

ESP32 firmware that reads temperature/humidity from an SHT31 sensor,
displays it on a 128x64 SSD1306 OLED, publishes over MQTT, and advertises
the same readings over BLE for a phone nearby (Chrome Web Bluetooth).

**Firmware version:** 1.4.0

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
| `Config.h` | Pins, targets, device identity, BLE UUIDs, timing (no hardcoded WiFi/MQTT host) |
| `Topics.h` | MQTT topic strings |
| `ProvisioningStore.h` / `.cpp` | Preferences: WiFi + MQTT host/port/user/pass/tls/mode |
| `GrowNetworkManager.h` / `.cpp` | SoftAP two-step portal (WiFi → MQTT), tests, reconnect |
| `MqttClient.h` / `.cpp` | Broker connect (plain or TLS), LWT/birth, publish JSON |
| `BleSensor.h` / `.cpp` | BLE GATT advertise + notify live T/H (NimBLE preferred) |
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

## BLE local sensor (Chrome Web Bluetooth)

Independent of WiFi/MQTT: when you are near the ESP, MycoMonitor can read
live temperature/humidity over Bluetooth. This does **not** replace MQTT
dashboard data — it is a local, phone-in-the-room path.

### Advertising

| Field | Value |
|---|---|
| Device name | `GrowOS-XXXX` (last 4 hex of MAC, same suffix style as SoftAP `GrowOS-Setup-XXXX`) |
| Service UUID | `6b6a0001-7c7a-4f3e-9b2d-1e5f8a9c0d01` |
| Characteristic UUID | `6b6a0002-7c7a-4f3e-9b2d-1e5f8a9c0d01` (read + notify) |

### Notify / read payload (UTF-8 JSON)

```json
{"t":29.7,"h":79.4,"id":"unit1"}
```

- `t` — temperature °C
- `h` — humidity % RH
- `id` — `DEVICE_ID` from `Config.h`

Sent on each valid SHT31 read (`SENSOR_READ_INTERVAL_MS`). Serial prints `BLE:on GrowOS-XXXX` at boot.

### Client requirements

- **Chrome only** (Android or desktop). No iOS Safari (no Web Bluetooth).
- **HTTPS (or localhost)** required — Web Bluetooth needs a secure context.
  Plain `http://192.168.x.x` will not expose `navigator.bluetooth`.
- Full phone/UI and mkcert/HTTPS LAN steps: see **MycoMonitor** docs
  (Local sensor / Connect local sensor).

### Stack

Prefers **NimBLE-Arduino** (`NimBLEDevice.h`) for WiFi coexistence; falls back
to ESP32 Bluedroid `BLEDevice` if NimBLE is not installed.

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
6. Serial 115200: SoftAP instructions, `BLE:on GrowOS-XXXX`, `MQTT connected to host:port`, `Published to MQTT`.

## Required libraries

- Adafruit GFX, SSD1306, SHT31
- PubSubClient, ArduinoJson
- **NimBLE-Arduino** (recommended; Library Manager: “NimBLE-Arduino” by h2zero)

`Preferences`, `WebServer`, `DNSServer`, `HTTPClient`, `WiFiClientSecure` ship with the ESP32 core.
Bluedroid BLE (`BLEDevice`) is used automatically if NimBLE is not present.

## Not built yet

- MQTT commands / relays
- Runtime target changes over MQTT
- OTA
- Cloud TLS CA pinning

See `wificonfig.md` for architecture notes (including BLE coexistence).
