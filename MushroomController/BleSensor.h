#ifndef BLE_SENSOR_H
#define BLE_SENSOR_H

#include "Sensor.h"

// Advertises live temperature/humidity over BLE GATT for Chrome Web Bluetooth.
// Device name: GrowOS-XXXX (last 4 hex of MAC, same suffix style as SoftAP).
class BleSensor {
public:
    BleSensor();

    // Starts NimBLE GATT service and advertising.
    // Call after the SHT31 is ready. Returns false if BLE init fails.
    // Requires NimBLE-Arduino (h2zero); compile fails if missing.
    bool begin();

    // Updates the readable characteristic and notifies subscribers.
    // No-op if begin() failed or the reading is invalid.
    void update(const SensorReading &reading);
    void update(float temperatureC, float humidityPct);

    bool isReady() const { return _ready; }

private:
    bool buildDeviceName();
    void setPayload(float temperatureC, float humidityPct);

    bool _ready;
    char _deviceName[16];
};

#endif // BLE_SENSOR_H
