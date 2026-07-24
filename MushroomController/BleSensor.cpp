#include "BleSensor.h"
#include "Config.h"
#include <WiFi.h>
#include <cstdio>
#include <cstring>

#if __has_include(<NimBLEDevice.h>)
#include <NimBLEDevice.h>
#define BLE_SENSOR_USE_NIMBLE 1
#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define BLE_SENSOR_USE_NIMBLE 0
#endif

#if BLE_SENSOR_USE_NIMBLE
static NimBLECharacteristic *s_char = nullptr;
#else
static BLECharacteristic *s_char = nullptr;
#endif

BleSensor::BleSensor() : _ready(false) {
    _deviceName[0] = '\0';
}

bool BleSensor::buildDeviceName() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(_deviceName, sizeof(_deviceName), "%s%02X%02X",
             BLE_DEVICE_NAME_PREFIX, mac[4], mac[5]);
    return true;
}

bool BleSensor::begin() {
    buildDeviceName();

#if BLE_SENSOR_USE_NIMBLE
    NimBLEDevice::init(_deviceName);
    // Prefer coexistence with STA WiFi + SoftAP when both are active.
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);

    NimBLEServer *server = NimBLEDevice::createServer();
    NimBLEService *service = server->createService(BLE_SERVICE_UUID);
    s_char = service->createCharacteristic(
        BLE_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    s_char->setValue("{}");
    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setName(_deviceName);
    advertising->setScanResponse(true);
    advertising->start();
#else
    BLEDevice::init(_deviceName);
    BLEServer *server = BLEDevice::createServer();
    BLEService *service = server->createService(BLE_SERVICE_UUID);
    s_char = service->createCharacteristic(
        BLE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    s_char->addDescriptor(new BLE2902());
    s_char->setValue("{}");
    service->start();

    // Device name was set in BLEDevice::init(); advertise service UUID.
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
#endif

    _ready = (s_char != nullptr);
    Serial.print(F("BLE:"));
    if (_ready) {
        Serial.print(F("on "));
        Serial.println(_deviceName);
    } else {
        Serial.println(F("init failed"));
    }
    return _ready;
}

void BleSensor::setPayload(float temperatureC, float humidityPct) {
    if (!_ready || s_char == nullptr) {
        return;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"t\":%.1f,\"h\":%.1f,\"id\":\"%s\"}",
             temperatureC, humidityPct, DEVICE_ID);

#if BLE_SENSOR_USE_NIMBLE
    s_char->setValue(reinterpret_cast<const uint8_t *>(buf), strlen(buf));
    s_char->notify();
#else
    s_char->setValue(reinterpret_cast<uint8_t *>(buf), strlen(buf));
    s_char->notify();
#endif
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
