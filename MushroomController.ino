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

static unsigned long lastSensorMs = 0;

static const char *netStateLabel(NetDisplayState state) {
    switch (state) {
        case NetDisplayState::SetupMode:
            return "Setup Mode";
        case NetDisplayState::Connecting:
            return "Connecting...";
        case NetDisplayState::Connected:
        default:
            return "Connected";
    }
}

// Mirror what the OLED shows into Serial Monitor.
static void printOledMirror(const SensorReading &reading, NetDisplayState netState, bool mqttOk) {
    float tempDelta = reading.temperature - TARGET_TEMPERATURE;
    float humDelta  = reading.humidity - TARGET_HUMIDITY;

    Serial.println(F("-------- OLED / console --------"));
    Serial.print(F("T "));
    Serial.print(reading.temperature, 1);
    Serial.print(F(" C   "));
    if (tempDelta >= 0) {
        Serial.print('+');
    }
    Serial.print(tempDelta, 1);
    Serial.println(F(" C"));

    Serial.print(F("H "));
    Serial.print(reading.humidity, 1);
    Serial.print(F(" %   "));
    if (humDelta >= 0) {
        Serial.print('+');
    }
    Serial.print(humDelta, 1);
    Serial.println(F(" % RH"));

    Serial.print(F("Status: "));
    Serial.print(netStateLabel(netState));
    if (netState == NetDisplayState::Connected) {
        Serial.print(F("  MQTT:"));
        Serial.print(mqttOk ? F("OK") : F("--"));
        Serial.print(F("  IP:"));
        Serial.print(WiFi.localIP());
    } else if (netState == NetDisplayState::SetupMode) {
        Serial.print(F("  SoftAP:"));
        Serial.print(growNetworkManager.setupApSsid());
        Serial.print(F("  clients:"));
        Serial.print(growNetworkManager.softApClientCount());
        Serial.print(F("  portal:http://"));
        Serial.print(WiFi.softAPIP());
    }
    Serial.println();
    Serial.println(F("--------------------------------"));
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(300);
    Serial.println();
    Serial.println(F("=== MushroomController boot ==="));
    Serial.print(F("Firmware "));
    Serial.println(FIRMWARE_VERSION);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (!display.begin()) {
        Serial.println(F("OLED Not Found"));
        while (true) { }
    }
    Serial.println(F("OLED OK"));

    if (!sensor.begin()) {
        Serial.println(F("SHT30 Not Found"));
        while (true) { }
    }
    Serial.println(F("Sensor OK"));

    growNetworkManager.begin();

    if (growNetworkManager.isConnected()) {
        Serial.println(F("WiFi connected — normal run"));
    } else if (growNetworkManager.isSetupMode()) {
        Serial.println(F("In WiFi Setup Mode — see SoftAP instructions above"));
    } else {
        Serial.println(F("WiFi connect timed out - will keep retrying in background"));
    }

    mqttClient.begin();
    lastSensorMs = millis();
}

void loop() {
    // Keep SoftAP portal responsive (do not block on long delays in Setup Mode).
    growNetworkManager.loop();

    if (growNetworkManager.isConnected()) {
        mqttClient.ensureConnected();
    }

    unsigned long now = millis();
    if (now - lastSensorMs < SENSOR_READ_INTERVAL_MS) {
        delay(growNetworkManager.isSetupMode() ? 2 : 10);
        return;
    }
    lastSensorMs = now;

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
        display.showReadings(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY,
                              netState, mqttClient.isConnected());
        printOledMirror(reading, netState, mqttClient.isConnected());

        if (growNetworkManager.isConnected()) {
            if (mqttClient.publishReading(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY)) {
                Serial.println(F("Published to MQTT"));
            } else {
                Serial.println(F("MQTT publish skipped (broker not connected)"));
            }
        }
    } else {
        Serial.println(F("Sensor read failed"));
        display.showError("Sensor Error");
    }
}
