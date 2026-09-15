#include <Wire.h>
/*
 * GrowOS / MushroomController
 *
 * REQUIRED Arduino IDE board settings (or Verify fails with "Sketch too big"):
 *   Tools → Board → ESP32 Dev Module
 *   Tools → Flash Size → 4MB (32Mb)
 *   Tools → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)
 *
 * Default partition is only ~1.31MB. This sketch is ~1.4–1.5MB (WiFi+BLE+MQTT).
 * With Huge APP, Maximum becomes ~3MB and the sketch fits (~45–50% used).
 */

#include "Config.h"
#include "Sensor.h"
#include "Display.h"
#include "GrowNetworkManager.h"
#include "MqttClient.h"
#include "BleSensor.h"
#include "StatusOutputs.h"

Sensor             sensor;
Display            display;
GrowNetworkManager growNetworkManager;
MqttClient         mqttClient;
BleSensor          bleSensor;
StatusOutputs      statusOutputs;

static unsigned long lastSensorMs = 0;
static unsigned long lastBleAdvMs = 0;

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
    if (bleSensor.isReady()) {
        Serial.print(F("  BLE:"));
        Serial.print(bleSensor.deviceName());
    }
    Serial.println();

    Serial.print(F("Relays: "));
    if (statusOutputs.isTestCycleMode()) {
        Serial.print(F("[TEST MODE] "));
        if (statusOutputs.testPhase() == StatusOutputs::TestCyclePhase::OnPhase) {
            if (statusOutputs.isTestBlockedByHumidity()) {
                Serial.print(F("ON-window (PAUSED: Humidity >= 95%)"));
            } else {
                Serial.print(F("ON-window (BOTH RELAYS ACTIVE)"));
            }
        } else {
            Serial.print(F("OFF-window (RELAYS OFF)"));
        }
        Serial.print(F(" rem:"));
        Serial.print(statusOutputs.testPhaseRemainingMs() / 1000);
        Serial.print(F("s"));
    } else {
        Serial.print(F("Humidifier:"));
        Serial.print(statusOutputs.isHumidifierOn() ? F("ON") : F("OFF"));
        Serial.print(F("  Fan:"));
        Serial.print(statusOutputs.isFanOn() ? F("ON") : F("OFF"));
        if (statusOutputs.isResting()) {
            Serial.print(F("  [REST: "));
            Serial.print(statusOutputs.restRemainingMs() / 1000);
            Serial.print(F("s rem]"));
        }
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

    statusOutputs.begin();

    // Start BLE before SoftAP — AP+STA often breaks advertising if BLE starts after.
    bleSensor.begin();

    growNetworkManager.begin();

    // SoftAP / WiFi mode changes can stop NimBLE advertising — restart it.
    bleSensor.restartAdvertising();

    if (growNetworkManager.isConnected()) {
        Serial.println(F("WiFi connected — normal run"));
    } else if (growNetworkManager.isSetupMode()) {
        Serial.println(F("In WiFi Setup Mode — see SoftAP instructions above"));
        Serial.println(F("BLE still advertises as GrowOS-XXXX (not GrowOS-Setup-XXXX)"));
    } else {
        Serial.println(F("WiFi connect timed out - will keep retrying in background"));
    }

    mqttClient.begin();

    lastSensorMs = millis();
}

void loop() {
    // Keep SoftAP portal responsive (do not block on long delays in Setup Mode).
    growNetworkManager.loop();

    // SoftAP can silently stop BLE advertising — refresh every 30s while in setup.
    if (growNetworkManager.isSetupMode() && bleSensor.isReady()) {
        unsigned long nowAdv = millis();
        if (nowAdv - lastBleAdvMs >= 30000) {
            lastBleAdvMs = nowAdv;
            bleSensor.restartAdvertising();
        }
    }

    if (growNetworkManager.isConnected()) {
        mqttClient.ensureConnected();
    }

    // Advance fan lag timers and write relay states on every loop iteration
    statusOutputs.loop();

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

        statusOutputs.update(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY);
        bleSensor.update(reading);

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
        statusOutputs.update(reading, TARGET_TEMPERATURE, TARGET_HUMIDITY);
    }
}
