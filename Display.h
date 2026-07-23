#ifndef DISPLAY_H
#define DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Sensor.h"

// OLED network status line (bottom of the readings screen).
enum class NetDisplayState {
    SetupMode,    // SoftAP portal active
    Connecting,   // STA join / reconnect in progress
    Connected     // STA up — show WiFi/MQTT OK/--
};

class Display {
public:
    Display();

    // Initializes the OLED over I2C. Returns false if not found.
    bool begin();

    // Renders the T / H readout with delta-from-target values, plus a
    // small network status line at the bottom, e.g.:
    //   T 31.3C        +3.3C
    //   H 79.5%      -10.5% RH
    //   WiFi:OK  MQTT:OK   |  Connecting...  |  WiFi:--  Setup Mode
    void showReadings(const SensorReading &reading, float targetTemp, float targetHumidity,
                       NetDisplayState netState, bool mqttConnected);

    // Renders a simple error message (used when the sensor read fails).
    void showError(const char *message);

private:
    Adafruit_SSD1306 _display;
};

#endif // DISPLAY_H
