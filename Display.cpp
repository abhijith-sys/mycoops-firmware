#include "Display.h"
#include "Config.h"
#include "Icons.h"
#include <math.h>
#include <stdio.h>

Display::Display() : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1) {
}

bool Display::begin() {
    if (!_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        return false;
    }
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.display();
    return true;
}

// Formats a signed delta like "+3.3" or "-10.5", with an optional suffix.
static void formatDelta(char *buf, size_t bufSize, float delta, const char *suffix) {
    char sign = (delta >= 0) ? '+' : '-';
    float absDelta = fabs(delta);
    snprintf(buf, bufSize, "%c%.1f%s", sign, absDelta, suffix);
}

void Display::showReadings(const SensorReading &reading, float targetTemp, float targetHumidity,
                            bool wifiConnected, bool mqttConnected) {
    float tempDelta     = reading.temperature - targetTemp;
    float humidityDelta = reading.humidity - targetHumidity;

    char tempDeltaStr[10];
    char humidityDeltaStr[14];
    formatDelta(tempDeltaStr, sizeof(tempDeltaStr), tempDelta, "");
    formatDelta(humidityDeltaStr, sizeof(humidityDeltaStr), humidityDelta, " RH");

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);

    // --- Temperature row: "T 31.3C        +3.3C" ---
    _display.drawBitmap(0, 0, thermometerIcon, 16, 16, SSD1306_WHITE);

    _display.setTextSize(2);
    _display.setCursor(20, 0);
    _display.print(reading.temperature, 1);
    _display.print("C");

    _display.setTextSize(1);
    _display.setCursor(86, 4);
    _display.print(tempDeltaStr);

    // --- Humidity row: "H 79.5%      -10.5% RH" ---
    _display.drawBitmap(0, 34, humidityIcon, 16, 16, SSD1306_WHITE);

    _display.setTextSize(2);
    _display.setCursor(20, 34);
    _display.print(reading.humidity, 1);
    _display.print("%");

    _display.setTextSize(1);
    _display.setCursor(70, 38);
    _display.print(humidityDeltaStr);

    // --- Status row: "WiFi:OK  MQTT:OK" ---
    _display.setTextSize(1);
    _display.setCursor(0, 55);
    _display.print("WiFi:");
    _display.print(wifiConnected ? "OK" : "--");
    _display.print("  MQTT:");
    _display.print(mqttConnected ? "OK" : "--");

    _display.display();
}

void Display::showError(const char *message) {
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    _display.setCursor(0, 28);
    _display.print(message);
    _display.display();
}