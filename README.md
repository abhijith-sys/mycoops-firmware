# MushroomController

ESP32 firmware that reads temperature/humidity from an SHT31 sensor,
displays it on a 128x64 SSD1306 OLED, publishes over MQTT, and advertises
the same readings over BLE for a phone nearby (Chrome Web Bluetooth).

**Firmware version:** 1.6.0

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
| `Config.h` | Pins, targets, device identity, BLE UUIDs, timing, `MQTT_ENABLE_CLOUD_WSS` (no hardcoded WiFi/MQTT host) |
| `Topics.h` | MQTT topic strings |
| `ProvisioningStore.h` / `.cpp` | Preferences: WiFi + MQTT host/port/user/pass/tls/mode |
| `GrowNetworkManager.h` / `.cpp` | SoftAP two-step portal (WiFi → MQTT), tests, reconnect |
| `MqttClient.h` / `.cpp` | Broker connect (plain, or TLS when enabled), LWT/birth, publish JSON |
| `BleSensor.h` / `.cpp` | BLE GATT advertise + notify live T/H (**NimBLE required**) |
| `StatusOutputs.h` / `.cpp` | Green/red climate LEDs (GPIO 12/13); helpers for future relays |
| `Sensor` / `Display` / `DeviceInfo` / `Icons` | Sensor, OLED, payload metadata |

## Status LEDs (4 LEDs)

| Metric | Green | Red | Meaning |
|---|---|---|---|
| **Temperature** | **GPIO 12** | **GPIO 13** | Green = in ideal band; red = too hot or too cold |
| **Humidity** | **GPIO 14** | **GPIO 15** | Green = in ideal band; red = too high or too low |

Ideal band uses `TARGET_TEMPERATURE` / `TARGET_HUMIDITY` plus tolerances in `Config.h`
(`CLIMATE_TEMP_TOLERANCE_C` = 1 °C, `CLIMATE_HUM_TOLERANCE_PCT` = 5 % RH).

Wiring each LED (active HIGH): `GPIO → 220Ω–1kΩ resistor → LED anode`, cathode → GND.

**GPIO 12 is a strapping pin** — do not leave it pulled HIGH at boot.

### Future humidifier / cooler (same logic, not LEDs)

Yes — you can reuse this control path later. Do **not** connect a humidifier
mains wire to the ESP. Use a **relay module** (or SSR) with optocoupler/transistor:

- `needsHumidifier()` is true when humidity is **below** the ideal band → relay ON  
- `needsCooling()` is true when temperature is **above** the ideal band → cooler/exhaust ON  

Next firmware step would add dedicated relay pins driven from those flags, with a
failsafe OFF when the sensor read fails.

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
3. **Step 2 — MQTT:** Local or Cloud mode, host/port/path/(user/pass).
   - `GET /suggest` — gateway IP + optional backend `/health` probe
   - **Test MQTT** — real CONNECT to the broker (TCP local or WSS cloud)
   - **Test backend** (local) — `GET http://{host}:4000/health`
4. **Save & Finish** — requires a successful MQTT test (re-runs on save if needed), then reboot.

### Local vs cloud

| Mode | Port default | Transport | Auth |
|---|---|---|---|
| Local | 1883 | Plain MQTT TCP (`PubSubClient`) | optional |
| Cloud | 443 | WSS `wss://host:443/mqtt` (ESP-IDF `esp_mqtt`, built into ESP32 core) | optional |

Cloud WSS is **on by default** (`MQTT_ENABLE_CLOUD_WSS 1` in `Config.h`) for Cloudflare
Tunnel setups. Set it to `0` for a smaller local-only flash image (SoftAP then shows
Local mode only).

No extra MQTT-WSS library install — uses Espressif’s `mqtt_client.h` from the ESP32
Arduino core. CA validation uses the ESP-IDF certificate bundle.

**Cloudflare Tunnel:** public hostname must be an **HTTP** service → `http://localhost:9002`
(Compose maps host 9002 → Mosquitto websockets `:9001`), **not** TCP → `:1883`. Client path: `/mqtt`.

### Preferences keys

`wifi_ssid`, `wifi_password`, `mqtt_host`, `mqtt_port`, `mqtt_user`,
`mqtt_pass`, `mqtt_path`, `mqtt_tls`, `mqtt_mode`.

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

**NimBLE-Arduino** (`NimBLEDevice.h`) only — required for WiFi coexistence and
flash size. There is no Bluedroid fallback; missing NimBLE fails the compile
with an install hint.

## MQTT topics

| Topic | Published | Contents |
|---|---|---|
| `farm/mushroom/unit1/sensors` | Every `SENSOR_READ_INTERVAL_MS` | Sensor + targets + device info |
| `farm/mushroom/unit1/status` | Connect (retained) + LWT | `{"online":true}` / `{"online":false}` |

## Setup (Arduino IDE)

1. Open `MushroomController/MushroomController.ino`.
2. Board: **ESP32 Dev Module**.
3. **Partition Scheme (required — this is why “105% / Sketch too big” happens):**  
   Look at the compile line `Maximum is 1310720 bytes` — that means you are still on the **default** partition.  
   Change it before Verify:
   - **Tools → Partition Scheme → `Huge APP (3MB No OTA/1MB SPIFFS)`**  
   After that, Maximum should be about **3145728** bytes (~3MB), and ~1.38MB will fit.  
   Also confirm **Tools → Flash Size → 4MB (32Mb)**.  
   A `partitions.csv` is in the sketch folder for Custom partition schemes if your board menu offers one.  
   Alternatives: `Minimal SPIFFS (1.9MB APP)` also works for this sketch size.  
   OTA is not used by this prototype yet.
4. In `Config.h`, set `DEVICE_ID` / `DEVICE_NAME` / targets as needed (not broker IP).
   Leave `MQTT_ENABLE_CLOUD_WSS` at `1` for Cloudflare WSS; set to `0` for local-only (smaller flash).
5. Upload.
6. Complete SoftAP steps above (or use MycoMonitor **ESP device setup** panel for the suggested host).
7. Serial 115200: SoftAP instructions, `BLE:on GrowOS-XXXX`, `MQTT connected to …`, `Published to MQTT`.

### Expected flash size

With **Huge APP**, **NimBLE**, and **`MQTT_ENABLE_CLOUD_WSS 0`**, the sketch (~1.3–1.4MB) fits
under the ~3MB app partition. With WSS enabled (~mbedTLS), expect a larger binary — still
within Huge APP. If Verify still shows `Maximum is 1310720`, the partition
menu was not changed — fix Tools → Partition Scheme first.

Backend health checks use plain `WiFiClient` (not `HTTPClient`) so ESP32 core 3.x does
not pull in `NetworkClientSecure` for the local health probe.

### Sync note

If you compile from another path (e.g. `Documents\VelonixTech\Projects\mycoops-firmware`),
copy this `MushroomController/` folder there after updates (`GrowNetworkManager.cpp`,
`Config.h`, `BleSensor.*`, `partitions.csv`).

## Required libraries

- Adafruit GFX, SSD1306, SHT31
- PubSubClient, ArduinoJson
- **NimBLE-Arduino** (**required**; Library Manager: “NimBLE-Arduino” by h2zero)

`Preferences`, `WebServer`, and `DNSServer` ship with the ESP32 core.
Cloud WSS uses Espressif **`mqtt_client.h` / `esp_mqtt`** (also part of the ESP32 core) —
no PsychicMqttClient or other third-party MQTT-WSS library.

## Not built yet

- MQTT commands / humidifier–cooler relays (LED logic is ready via `StatusOutputs`)
- Runtime target changes over MQTT
- OTA
- Cloud WSS CA pinning (currently ESP-IDF CA bundle)

See `wificonfig.md` for architecture notes (including BLE coexistence).
