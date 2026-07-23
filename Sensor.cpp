#include "Sensor.h"
#include "Config.h"
#include <Arduino.h>

Sensor::Sensor() : _sht31() {
}

bool Sensor::begin() {
    return _sht31.begin(SHT31_ADDR);
}

SensorReading Sensor::read() {
    SensorReading reading;
    reading.temperature = _sht31.readTemperature();
    reading.humidity    = _sht31.readHumidity();
    reading.valid        = !isnan(reading.temperature) && !isnan(reading.humidity);
    return reading;
}

void SensorReading::fill(JsonDocument &doc) const {
    doc["temperature"] = temperature;
    doc["humidity"]    = humidity;
    doc["sensor_ok"]   = valid;
}