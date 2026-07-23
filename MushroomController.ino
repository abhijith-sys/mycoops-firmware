#include <Wire.h>
#include "Config.h"
#include "Sensor.h"
#include "Display.h"
#include "WifiManager.h"
#include "MqttClient.h"

Sensor      sensor;
Display     display;
WifiManager wifiManager;
MqttClient  mqttClient;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!display.begin()) {
        Serial.println("OLED Not Found");
        while (true) { }
    }

    if (!sensor.begin()) {
        Serial.println("SHT30 Not Found");
        while (true) { }
    }

    if (wifiManager.begin()) {
        Serial.println("WiFi connected");
    } else {
        Serial.println("WiFi connect timed out - will keep retrying in background");
    }

    mqttClient.begin();
}

void loop() {
    wifiManager.ensureConnected();
    mqttClient.ensureConnected();

    SensorReading reading = sensor.read();

    if (reading.valid) {
        Serial.print("Temperature : ");
        Serial.print(reading.temperature, 1);
        Serial.println(" C");

        Serial.print("Humidity    : ");
        Serial.print(reading.humidity, 1);
        Serial.println(" %");

        Serial.println("----------------------");

        display.showReadings(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY,
                              wifiManager.isConnected(), mqttClient.isConnected());

        if (mqttClient.publishReading(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY)) {
            Serial.println("Published to MQTT");
        } else {
            Serial.println("MQTT publish skipped (broker not connected)");
        }
    } else {
        Serial.println("Sensor read failed");
        display.showError("Sensor Error");
    }

    delay(SENSOR_READ_INTERVAL_MS);
}
