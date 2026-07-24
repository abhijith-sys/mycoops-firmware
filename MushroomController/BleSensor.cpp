#include "BleSensor.h"
#include "Config.h"
#include <WiFi.h>
#include <cstdio>
#include <cstring>

#if !__has_include(<NimBLEDevice.h>)
#error "NimBLE-Arduino is required. Install via Library Manager: NimBLE-Arduino by h2zero."
#endif

#include <NimBLEDevice.h>

#if __has_include(<esp_coexist.h>)
#include <esp_coexist.h>
#endif

static NimBLECharacteristic *s_char = nullptr;

BleSensor::BleSensor() : _ready(false) {
    _deviceName[0] = '\0';
}

bool BleSensor::buildDeviceName() {
    uint8_t mac[6];
    // Use STA MAC even if SoftAP is up (SoftAP MAC can differ).
    WiFi.macAddress(mac);
    snprintf(_deviceName, sizeof(_deviceName), "%s%02X%02X",
             BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);
    return true;
}

void BleSensor::startAdvertising() {
    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    if (advertising == nullptr) {
        return;
    }

    advertising->stop();
    delay(20);

    // Legacy ADV is only 31 bytes. Name (~11) + 128-bit UUID (18) + flags overflow
    // that limit and advertising silently fails — keep them in separate packets.
    NimBLEAdvertisementData advData;
    advData.setFlags(0x06); // general discoverable, BR/EDR not supported
    advData.setName(_deviceName);
    advertising->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
    advertising->setScanResponseData(scanData);

    bool started = advertising->start();
    Serial.print(F("BLE advertising: "));
    Serial.print(_deviceName);
    Serial.println(started ? F(" OK") : F(" FAILED"));
    Serial.println(F("(Phone Bluetooth Settings often hide BLE GATT — use nRF Connect or MycoMonitor Local sensor)"));
}

bool BleSensor::begin() {
    buildDeviceName();

#if defined(ESP_COEX_PREFER_BALANCE)
    // SoftAP + STA share the radio with BLE; prefer balanced coexistence.
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
#endif

    NimBLEDevice::init(_deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer *server = NimBLEDevice::createServer();
    NimBLEService *service = server->createService(BLE_SERVICE_UUID);
    s_char = service->createCharacteristic(
        BLE_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_char->setValue("{}");
    service->start();

    startAdvertising();

    _ready = (s_char != nullptr);
    Serial.print(F("BLE:"));
    if (_ready) {
        Serial.print(F("on "));
        Serial.println(_deviceName);
        Serial.println(F("Look for BLE name GrowOS-XXXX — not WiFi SoftAP GrowOS-Setup-XXXX"));
    } else {
        Serial.println(F("init failed"));
    }
    return _ready;
}

void BleSensor::restartAdvertising() {
    if (!_ready) {
        return;
    }
    startAdvertising();
}

void BleSensor::setPayload(float temperatureC, float humidityPct) {
    if (!_ready || s_char == nullptr) {
        return;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"t\":%.1f,\"h\":%.1f,\"id\":\"%s\"}",
             temperatureC, humidityPct, DEVICE_ID);

    s_char->setValue(reinterpret_cast<const uint8_t *>(buf), strlen(buf));
    s_char->notify();
}

void BleSensor::update(const SensorReading &reading) {
    if (!reading.valid) {
        return;
    }
    update(reading.temperature, reading.humidity);
}

void BleSensor::update(float temperatureC, float humidityPct) {
    setPayload(temperatureC, humidityPct);
}
