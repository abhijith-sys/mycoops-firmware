#include "StatusOutputs.h"
#include "Config.h"
#include <Arduino.h>

static void writePair(uint8_t greenPin, uint8_t redPin, bool ideal) {
    digitalWrite(greenPin, ideal ? HIGH : LOW);
    digitalWrite(redPin, ideal ? LOW : HIGH);
}

void StatusOutputs::begin() {
    _needsHumidifier = false;
    _needsCooling = false;

    pinMode(TEMP_LED_GREEN_PIN, OUTPUT);
    pinMode(TEMP_LED_RED_PIN, OUTPUT);
    pinMode(HUM_LED_GREEN_PIN, OUTPUT);
    pinMode(HUM_LED_RED_PIN, OUTPUT);

    digitalWrite(TEMP_LED_GREEN_PIN, LOW);
    digitalWrite(TEMP_LED_RED_PIN, LOW);
    digitalWrite(HUM_LED_GREEN_PIN, LOW);
    digitalWrite(HUM_LED_RED_PIN, LOW);

    Serial.println(F("Status LEDs:"));
    Serial.println(F("  Temp green=GPIO12 red=GPIO13"));
    Serial.println(F("  Hum  green=GPIO14 red=GPIO15"));
}

void StatusOutputs::update(const SensorReading &reading, float targetTempC, float targetHumidityPct) {
    if (!reading.valid) {
        digitalWrite(TEMP_LED_GREEN_PIN, LOW);
        digitalWrite(TEMP_LED_RED_PIN, LOW);
        digitalWrite(HUM_LED_GREEN_PIN, LOW);
        digitalWrite(HUM_LED_RED_PIN, LOW);
        _needsHumidifier = false;
        _needsCooling = false;
        return;
    }

    const float t = reading.temperature;
    const float h = reading.humidity;

    const bool tempHigh = t > (targetTempC + CLIMATE_TEMP_TOLERANCE_C);
    const bool tempLow  = t < (targetTempC - CLIMATE_TEMP_TOLERANCE_C);
    const bool humHigh  = h > (targetHumidityPct + CLIMATE_HUM_TOLERANCE_PCT);
    const bool humLow   = h < (targetHumidityPct - CLIMATE_HUM_TOLERANCE_PCT);

    const bool tempIdeal = !tempHigh && !tempLow;
    const bool humIdeal  = !humHigh && !humLow;

    _needsCooling = tempHigh;
    _needsHumidifier = humLow;

    writePair(TEMP_LED_GREEN_PIN, TEMP_LED_RED_PIN, tempIdeal);
    writePair(HUM_LED_GREEN_PIN, HUM_LED_RED_PIN, humIdeal);
}
