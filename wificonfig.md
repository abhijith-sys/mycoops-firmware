# WiFi + MQTT Configuration Architecture

Production SoftAP provisioning for GrowOS / MushroomController.

## Goals

- Never hardcode WiFi or MQTT broker host in the firmware binary.
- Store WiFi + MQTT settings in ESP32 flash (`Preferences`).
- Two-step SoftAP wizard: WiFi first, then MQTT (with live tests).
- Local (LAN Mosquitto) and Cloud (TLS) from one firmware image.
- Recover without USB: fail-into-setup after repeated STA failures; missing MQTT host also enters setup.

---

## Boot Flow

1. Device boots; load Preferences.
2. If no WiFi → SoftAP step 1.
3. If WiFi OK but no `mqtt_host` → SoftAP step 2 (AP+STA, keep LAN).
4. If both OK → STA + MQTT from Preferences.
5. If STA fails N times → SoftAP step 1.

---

## SoftAP portal

SSID: `GrowOS-Setup-XXXX` (MAC), IP `192.168.4.1`.

| Route | Purpose |
|-------|---------|
| `GET /` | Step 1 or 2 HTML |
| `POST /save-wifi` | Save WiFi, join STA, stay on SoftAP → step 2 |
| `GET /suggest` | STA/gateway IPs + optional backend health hint |
| `POST /test-mqtt` | MQTT CONNECT probe → `{ok,error}` |
| `POST /test-backend` | `GET http://host:4000/health` |
| `POST /save-mqtt` | Require MQTT test OK, save, reboot |

**Dashboard hint:** MycoMonitor `GET /api/setup/connection` returns suggested `mqtt_host` from the browser Host header. Copy **before** joining SoftAP (phone on SoftAP cannot reach the LAN dashboard).

---

## Flash Storage

```
wifi_ssid, wifi_password
mqtt_host, mqtt_port, mqtt_user, mqtt_pass, mqtt_tls, mqtt_mode
```

Do NOT store credentials in `Config.h`.

---

## Class Responsibilities

`GrowNetworkManager` — SoftAP portal, WiFi connect/reconnect, setup gating.  
`ProvisioningStore` — Preferences load/save.  
`MqttClient` — broker connect (plain / TLS `setInsecure`), publish.  

Rest of firmware never calls `WiFi.*` for provisioning (DeviceInfo may read IP/RSSI when connected).

Example:

```cpp
growNetworkManager.begin();
growNetworkManager.loop();
if (growNetworkManager.isConnected()) {
    mqttClient.ensureConnected();
}
```

---

## OLED Status

```
WiFi ✓ / MQTT ✓
WiFi ✗ / Setup Mode
Connecting...
```

---

## Future

- OTA updates
- Cloud TLS CA pinning (replace `setInsecure`)
- HTTPS configuration portal
- Device hostname / NTP
- Remote MQTT re-provisioning
