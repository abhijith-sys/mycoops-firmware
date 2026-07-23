#include <Wire.h>
#include "Config.h"
#include "Sensor.h"
#include "Display.h"
#include "GrowNetworkManager.h"
#include "MqttClient.h"

Sensor             sensor;
Display            display;
GrowNetworkManager growNetworkManager;
MqttClient         mqttClient;

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

    growNetworkManager.begin();

    if (growNetworkManager.isConnected()) {
        Serial.println("WiFi connected");
    } else if (growNetworkManager.isSetupMode()) {
        Serial.println("In WiFi Setup Mode — configure via SoftAP portal");
    } else {
        Serial.println("WiFi connect timed out - will keep retrying in background");
    }

    mqttClient.begin();
}

void loop() {
    growNetworkManager.loop();

    // Skip MQTT while the SoftAP portal is active (no LAN / broker path).
    if (growNetworkManager.isConnected()) {
        mqttClient.ensureConnected();
    }

    SensorReading reading = sensor.read();

    NetDisplayState netState;
    if (growNetworkManager.isSetupMode()) {
        netState = NetDisplayState::SetupMode;
    } else if (growNetworkManager.isConnected()) {
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

        if (growNetworkManager.isConnected()) {
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
