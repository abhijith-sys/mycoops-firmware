# WiFi Configuration Architecture

Replace the current hardcoded WiFi credentials with a professional first-time setup flow similar to commercial IoT devices.

## Goals

- Never hardcode WiFi credentials in the firmware.
- Store WiFi credentials in ESP32 flash memory using the `Preferences` library.
- On first boot (or if no valid credentials exist), automatically enter WiFi Setup Mode.
- If connection to the saved WiFi fails repeatedly, automatically return to Setup Mode.
- The firmware should recover without requiring a USB cable or recompilation.

---

## Boot Flow

1. Device boots.
2. Load WiFi credentials from flash storage.
3. If credentials exist:
   - Attempt to connect to WiFi.
   - If successful, continue to MQTT.
4. If credentials do not exist:
   - Start Setup Mode.
5. If WiFi cannot be reached after several retries:
   - Return to Setup Mode automatically.

---

## Setup Mode

When in Setup Mode, the ESP32 should:

- Create its own WiFi Access Point.
- SSID format:

```
GrowOS-Setup-XXXX
```

where `XXXX` is derived from the ESP32 MAC address to make each device unique.

Default AP IP:

```
192.168.4.1
```

The ESP32 should host a small configuration website.

---

## Configuration Web Page

The setup page should:

- Display nearby WiFi networks (scan using `WiFi.scanNetworks()`).
- Allow manual SSID entry if the desired network is hidden.
- Allow password entry.
- Save credentials to flash memory using `Preferences`.
- Attempt connection.
- Show success/failure.
- Restart automatically after successful configuration.

---

## Flash Storage

Use the ESP32 `Preferences` library.

Store:

```
wifi_ssid
wifi_password
```

Do NOT store credentials in Config.h.

---

## Connection Management

The WiFi manager should automatically:

- reconnect after connection loss
- avoid blocking the main loop
- retry using exponential backoff or a fixed retry interval
- never freeze the firmware

---

## Class Responsibilities

Rename `WifiManager` to `GrowNetworkManager` (not `NetworkManager` — that name conflicts with ESP32 Arduino Core 3.x).

Responsibilities:

- Load credentials
- Save credentials
- Connect to WiFi
- Auto reconnect
- Detect first boot
- Detect invalid credentials
- Start Setup Portal
- Stop Setup Portal after configuration
- Report connection status

The rest of the firmware should never interact with the WiFi library directly.

Example:

```cpp
growNetworkManager.begin();

growNetworkManager.loop();

if (growNetworkManager.isConnected())
{
    mqttClient.loop();
}
```

---

## OLED Status

Display connection status on the OLED.

Examples:

```
WiFi ✓
MQTT ✓
```

or

```
WiFi ✗
Setup Mode
```

or

```
Connecting...
```

---

## Future Compatibility

Design GrowNetworkManager so future features can be added without changing the rest of the firmware:

- OTA updates
- Ethernet
- Static IP
- Multiple WiFi profiles
- Remote network diagnostics
- HTTPS configuration portal
- Device hostname
- NTP time synchronization

The network manager should be completely isolated from Sensor, Display, and MQTT classes following the Single Responsibility Principle.