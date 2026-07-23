#include <Wire.h>
#include "Config.h"
#include "Sensor.h"
#include "Display.h"
#include "NetworkManager.h"
#include "MqttClient.h"

Sensor         sensor;
Display        display;
NetworkManager networkManager;
MqttClient     mqttClient;

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

    networkManager.begin();

    if (networkManager.isConnected()) {
        Serial.println("WiFi connected");
    } else if (networkManager.isSetupMode()) {
        Serial.println("In WiFi Setup Mode — configure via SoftAP portal");
    } else {
        Serial.println("WiFi connect timed out - will keep retrying in background");
    }

    mqttClient.begin();
}

void loop() {
    networkManager.loop();

    // Skip MQTT while the SoftAP portal is active (no LAN / broker path).
    if (networkManager.isConnected()) {
        mqttClient.ensureConnected();
    }

    SensorReading reading = sensor.read();

    NetDisplayState netState;
    if (networkManager.isSetupMode()) {
        netState = NetDisplayState::SetupMode;
    } else if (networkManager.isConnected()) {
        netState = NetDisplayState::Connected;
    } else {
        netState = NetDisplayState::Connecting;
    }

    if (reading.valid) {
        Serial.print("Temperature : ");
        Serial.print(reading.temperature, 1);
        Serial.println(" C");

        Serial.print("Humidity    : ");
        Serial.print(reading.humidity, 1);
        Serial.println(" %");

        Serial.println("----------------------");

        display.showReadings(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY,
                              netState, mqttClient.isConnected());

        if (networkManager.isConnected()) {
            if (mqttClient.publishReading(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY)) {
                Serial.println("Published to MQTT");
            } else {
                Serial.println("MQTT publish skipped (broker not connected)");
            }
        }
    } else {
        Serial.println("Sensor read failed");
        display.showError("Sensor Error");
    }

    delay(SENSOR_READ_INTERVAL_MS);
}
