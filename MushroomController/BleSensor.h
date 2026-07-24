#ifndef BLE_SENSOR_H
#define BLE_SENSOR_H

#include "Sensor.h"

// Advertises live temperature/humidity over BLE GATT for Chrome Web Bluetooth.
// Device name: GrowOS-XXXX (NOT GrowOS-Setup-XXXX — that is SoftAP WiFi only).
class BleSensor {
public:
    BleSensor();

    // Starts NimBLE GATT + advertising. Prefer calling BEFORE SoftAP/WiFi AP mode.
    bool begin();

    // Call after SoftAP / WiFi mode changes — SoftAP often stops BLE advertising.
    void restartAdvertising();

    void update(const SensorReading &reading);
    void update(float temperatureC, float humidityPct);

    bool isReady() const { return _ready; }
    const char *deviceName() const { return _deviceName; }

private:
    bool buildDeviceName();
    void setPayload(float temperatureC, float humidityPct);
    void startAdvertising();

    bool _ready;
    char _deviceName[16];
};

#endif // BLE_SENSOR_H
