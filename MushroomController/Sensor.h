#ifndef SENSOR_H
#define SENSOR_H

#include <Adafruit_SHT31.h>
#include <ArduinoJson.h>

// Holds one reading from the sensor
struct SensorReading {
    float temperature;   // deg C
    float humidity;       // % RH
    bool  valid;           // false if the read failed (NaN from sensor)

    // Adds this reading's fields to a JSON document. Keeps MqttClient
    // from needing to know about temperature/humidity/sensor_ok directly -
    // useful once more sensors (CO2, light, water level) are added.
    void fill(JsonDocument &doc) const;
};

class Sensor {
public:
    Sensor();

    // Initializes the SHT31 sensor over I2C. Returns false if not found.
    bool begin();

    // Takes a fresh reading. Check reading.valid before using the values.
    SensorReading read();

private:
    Adafruit_SHT31 _sht31;
};

#endif // SENSOR_H