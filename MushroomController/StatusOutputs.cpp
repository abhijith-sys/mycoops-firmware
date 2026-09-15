#include "StatusOutputs.h"
#include "Config.h"
#include <Arduino.h>

static void writePair(uint8_t greenPin, uint8_t redPin, bool ideal) {
    digitalWrite(greenPin, ideal ? HIGH : LOW);
    digitalWrite(redPin, ideal ? LOW : HIGH);
}

void StatusOutputs::setRelay(uint8_t pin, bool on) {
#if RELAY_ACTIVE_LOW
    digitalWrite(pin, on ? LOW : HIGH);
#else
    digitalWrite(pin, on ? HIGH : LOW);
#endif
}

unsigned long StatusOutputs::restRemainingMs() const {
    if (_testCycleMode || _relayState != RelayState::RestCooldown) {
        return 0;
    }
    unsigned long elapsed = millis() - _restTimerStartMs;
    if (elapsed >= HUMIDIFIER_REST_INTERVAL_MS) {
        return 0;
    }
    return (HUMIDIFIER_REST_INTERVAL_MS - elapsed);
}

unsigned long StatusOutputs::continuousRunMs() const {
    if (_testCycleMode) {
        if (!_humidifierOn) return 0;
        return (millis() - _testPhaseStartMs);
    }
    if (!_humidifierOn) {
        return 0;
    }
    return (millis() - _humidifierRunStartMs);
}

void StatusOutputs::setTestCycleMode(bool enabled) {
    _testCycleMode = enabled;
    _testPhaseStartMs = millis();
    _testPhase = TestCyclePhase::OnPhase;
    _testBlockedByHumidity = false;
    _humidifierOn = false;
    _fanOn = false;
    setRelay(HUMIDIFIER_RELAY_PIN, false);
    setRelay(FAN_RELAY_PIN, false);
    _relayState = RelayState::Idle;
    Serial.print(F("Relay Mode switched to: "));
    Serial.println(_testCycleMode ? F("TEST CYCLE MODE") : F("PRODUCTION THRESHOLD MODE"));
}

void StatusOutputs::setTestIntervals(unsigned long onMs, unsigned long offMs) {
    _testOnDurationMs = onMs;
    _testOffDurationMs = offMs;
    Serial.print(F("Test intervals updated: ON="));
    Serial.print(_testOnDurationMs / 1000);
    Serial.print(F("s, OFF="));
    Serial.print(_testOffDurationMs / 1000);
    Serial.println(F("s"));
}

unsigned long StatusOutputs::testPhaseRemainingMs() const {
    if (!_testCycleMode) {
        return 0;
    }
    unsigned long elapsed = millis() - _testPhaseStartMs;
    unsigned long duration = (_testPhase == TestCyclePhase::OnPhase) ? _testOnDurationMs : _testOffDurationMs;
    if (elapsed >= duration) {
        return 0;
    }
    return (duration - elapsed);
}

void StatusOutputs::begin() {
    _needsHumidifier = false;
    _needsCooling = false;
    _relayState = RelayState::Idle;
    _fanTimerStartMs = 0;
    _humidifierRunStartMs = 0;
    _restTimerStartMs = 0;
    _enteringRest = false;
    _humidifierOn = false;
    _fanOn = false;

    // Test cycle mode state initialized from Config.h
    _testCycleMode = RELAY_TEST_CYCLE_MODE;
    _testOnDurationMs = RELAY_TEST_ON_DURATION_MS;
    _testOffDurationMs = RELAY_TEST_OFF_DURATION_MS;
    _testPhase = TestCyclePhase::OnPhase;
    _testPhaseStartMs = millis();
    _testBlockedByHumidity = false;
    _lastHumidity = 0.0f;
    _lastReadingValid = false;
    _consecutiveFailedReads = 0;

    pinMode(TEMP_LED_GREEN_PIN, OUTPUT);
    pinMode(TEMP_LED_RED_PIN, OUTPUT);
    pinMode(HUM_LED_GREEN_PIN, OUTPUT);
    pinMode(HUM_LED_RED_PIN, OUTPUT);

    digitalWrite(TEMP_LED_GREEN_PIN, LOW);
    digitalWrite(TEMP_LED_RED_PIN, LOW);
    digitalWrite(HUM_LED_GREEN_PIN, LOW);
    digitalWrite(HUM_LED_RED_PIN, LOW);

    // Ensure relay pins are set to OFF before driving pinMode as OUTPUT (prevents boot click)
    setRelay(HUMIDIFIER_RELAY_PIN, false);
    setRelay(FAN_RELAY_PIN, false);
    pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
    pinMode(FAN_RELAY_PIN, OUTPUT);
    setRelay(HUMIDIFIER_RELAY_PIN, false);
    setRelay(FAN_RELAY_PIN, false);

    Serial.println(F("Status LEDs:"));
    Serial.println(F("  Temp green=GPIO12 red=GPIO13"));
    Serial.println(F("  Hum  green=GPIO14 red=GPIO15"));
    Serial.print(F("Relays ("));
    Serial.print(RELAY_ACTIVE_LOW ? F("active LOW") : F("active HIGH"));
    Serial.println(F("):"));
    Serial.println(F("  Humidifier=GPIO25  Fan=GPIO26"));

    if (_testCycleMode) {
        Serial.println(F("  Mode: *** TEST CYCLE MODE ACTIVE ***"));
        Serial.print(F("  Cycle: "));
        Serial.print(_testOnDurationMs / 1000);
        Serial.print(F("s ("));
        Serial.print(_testOnDurationMs / 60000);
        Serial.print(F("m) ON / "));
        Serial.print(_testOffDurationMs / 1000);
        Serial.print(F("s ("));
        Serial.print(_testOffDurationMs / 60000);
        Serial.println(F("m) OFF alternating"));
        Serial.println(F("  Safety: Will NOT start ON cycle if humidity >= 95.0%"));
    } else {
        Serial.println(F("  Mode: PRODUCTION THRESHOLD MODE"));
        Serial.println(F("  Cutoff: stop >=95.0%, start <92.0%"));
        Serial.println(F("  Safety: 3hr max continuous run, 30min rest cooldown"));
    }
}

void StatusOutputs::loop() {
    unsigned long now = millis();

    // -------------------------------------------------------------
    // Test Cycle Mode: 30min ON / 30min OFF alternating
    // -------------------------------------------------------------
    if (_testCycleMode) {
        if (_testPhase == TestCyclePhase::OnPhase) {
            if (now - _testPhaseStartMs >= _testOnDurationMs) {
                _testPhase = TestCyclePhase::OffPhase;
                _testPhaseStartMs = now;
                _humidifierOn = false;
                _fanOn = false;
                _testBlockedByHumidity = false;
                setRelay(HUMIDIFIER_RELAY_PIN, false);
                setRelay(FAN_RELAY_PIN, false);

                Serial.println();
                Serial.println(F("=================================================="));
                Serial.println(F(">>> [RELAY STOP] Humidifier & Fan turned OFF"));
                Serial.print(F("    Reason: "));
                Serial.print(_testOnDurationMs / 1000);
                Serial.println(F("s ON cycle completed"));
                Serial.print(F("    Next: Entering "));
                Serial.print(_testOffDurationMs / 1000);
                Serial.print(F("s ("));
                Serial.print(_testOffDurationMs / 60000);
                Serial.println(F("m) OFF cycle"));
                Serial.println(F("=================================================="));
                Serial.println();
            }
        } else { // OffPhase
            if (now - _testPhaseStartMs >= _testOffDurationMs) {
                _testPhase = TestCyclePhase::OnPhase;
                _testPhaseStartMs = now;

                Serial.println();
                Serial.println(F("=================================================="));
                Serial.print(F(">>> [TEST MODE] "));
                Serial.print(_testOffDurationMs / 1000);
                Serial.println(F("s OFF cycle completed -> Starting ON window"));

                // Check humidity before starting: if >= 95%, do not start relays
                if (_lastReadingValid && _lastHumidity >= HUMIDIFIER_OFF_ABOVE_PCT) {
                    _testBlockedByHumidity = true;
                    _humidifierOn = false;
                    _fanOn = false;
                    setRelay(HUMIDIFIER_RELAY_PIN, false);
                    setRelay(FAN_RELAY_PIN, false);
                    Serial.println(F(">>> [RELAY START BLOCKED - HUMIDITY >= 95%]"));
                    Serial.print(F("    Current Humidity: "));
                    Serial.print(_lastHumidity, 1);
                    Serial.println(F("% (>= 95.0% threshold)"));
                    Serial.println(F("    Humidifier & Fan NOT started (paused until humidity < 95%)"));
                    Serial.println(F("=================================================="));
                    Serial.println();
                } else {
                    _testBlockedByHumidity = false;
                    _humidifierOn = true;
                    _fanOn = true;
                    setRelay(HUMIDIFIER_RELAY_PIN, true);
                    setRelay(FAN_RELAY_PIN, true);
                    Serial.println(F(">>> [RELAY START] Humidifier & Fan turned ON"));
                    Serial.print(F("    Duration: "));
                    Serial.print(_testOnDurationMs / 1000);
                    Serial.print(F("s ("));
                    Serial.print(_testOnDurationMs / 60000);
                    Serial.println(F("m)"));
                    Serial.print(F("    Humidity: "));
                    if (_lastReadingValid) {
                        Serial.print(_lastHumidity, 1);
                        Serial.println(F("% (< 95.0% OK)"));
                    } else {
                        Serial.println(F("pending sensor read"));
                    }
                    Serial.println(F("=================================================="));
                    Serial.println();
                }
            }
        }
        return;
    }

    // -------------------------------------------------------------
    // Production Mode: Threshold and safety state machine
    // -------------------------------------------------------------
    // Check maximum continuous run time limit (3 hours)
    if (_relayState == RelayState::HumidOnFanWait || _relayState == RelayState::BothOn) {
        if (now - _humidifierRunStartMs >= HUMIDIFIER_MAX_CONTINUOUS_RUN_MS) {
            Serial.println(F("Relay: 3-hour continuous run limit reached! Shutting down for 30-min rest"));
            _humidifierOn = false;
            setRelay(HUMIDIFIER_RELAY_PIN, false);

            if (_fanOn) {
                // Keep fan on for 5s mist clearing, then transition to RestCooldown
                _enteringRest = true;
                _fanTimerStartMs = now;
                _relayState = RelayState::FanOffWait;
                Serial.println(F("Relay: Fan clearing mist for 5s before 30-min rest cooldown..."));
            } else {
                // Fan never started; enter RestCooldown immediately
                _enteringRest = false;
                _restTimerStartMs = now;
                _relayState = RelayState::RestCooldown;
                Serial.println(F("Relay: Entering 30-minute rest cooldown"));
            }
            return;
        }
    }

    switch (_relayState) {
        case RelayState::HumidOnFanWait:
            if (now - _fanTimerStartMs >= FAN_LAG_AFTER_HUMIDIFIER_MS) {
                _fanOn = true;
                _relayState = RelayState::BothOn;
                setRelay(FAN_RELAY_PIN, true);
                Serial.println(F("Relay: Fan ON (start lag elapsed)"));
            }
            break;

        case RelayState::FanOffWait:
            if (now - _fanTimerStartMs >= FAN_LAG_AFTER_HUMIDIFIER_MS) {
                _fanOn = false;
                setRelay(FAN_RELAY_PIN, false);

                if (_enteringRest) {
                    _enteringRest = false;
                    _restTimerStartMs = now;
                    _relayState = RelayState::RestCooldown;
                    Serial.println(F("Relay: Fan OFF. Entering 30-minute rest cooldown"));
                } else {
                    _relayState = RelayState::Idle;
                    Serial.println(F("Relay: Fan OFF (mist cleared, idle)"));
                }
            }
            break;

        case RelayState::RestCooldown:
            if (now - _restTimerStartMs >= HUMIDIFIER_REST_INTERVAL_MS) {
                _relayState = RelayState::Idle;
                _humidifierRunStartMs = 0;
                Serial.println(F("Relay: 30-minute rest cooldown completed. System ready"));
            }
            break;

        case RelayState::Idle:
        case RelayState::BothOn:
        default:
            break;
    }
}

void StatusOutputs::update(const SensorReading &reading, float targetTempC, float targetHumidityPct) {
    if (!reading.valid) {
        _consecutiveFailedReads++;
        // Require 3 consecutive failed cycles (6 seconds) before cutting relays,
        // so an isolated 1-sample I2C read glitch does not toggle relays OFF and ON.
        if (_consecutiveFailedReads >= 3) {
            digitalWrite(TEMP_LED_GREEN_PIN, LOW);
            digitalWrite(TEMP_LED_RED_PIN, LOW);
            digitalWrite(HUM_LED_GREEN_PIN, LOW);
            digitalWrite(HUM_LED_RED_PIN, LOW);
            _needsHumidifier = false;
            _needsCooling = false;
            _lastReadingValid = false;

            // Failsafe: immediately shut off both relays
            if (_humidifierOn || _fanOn) {
                Serial.println(F("Relay: Failsafe triggered by sustained sensor failure (3 cycles) -> relays OFF"));
            }
            _humidifierOn = false;
            _fanOn = false;
            setRelay(HUMIDIFIER_RELAY_PIN, false);
            setRelay(FAN_RELAY_PIN, false);

            // Maintain RestCooldown if currently cooling down so invalid reads do not bypass safety
            if (!_testCycleMode && _relayState != RelayState::RestCooldown) {
                _relayState = RelayState::Idle;
                _enteringRest = false;
            }
        }
        return;
    }

    _consecutiveFailedReads = 0;

    const float t = reading.temperature;
    const float h = reading.humidity;

    _lastReadingValid = true;
    _lastHumidity = h;

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

    // -------------------------------------------------------------
    // Test Cycle Mode: 30min ON / 30min OFF with >=95% humidity safety check
    // -------------------------------------------------------------
    if (_testCycleMode) {
        if (_testPhase == TestCyclePhase::OnPhase) {
            if (h >= HUMIDIFIER_OFF_ABOVE_PCT) {
                // Above 95% -> do not run relays
                if (_humidifierOn || _fanOn) {
                    _humidifierOn = false;
                    _fanOn = false;
                    _testBlockedByHumidity = true;
                    setRelay(HUMIDIFIER_RELAY_PIN, false);
                    setRelay(FAN_RELAY_PIN, false);

                    Serial.println();
                    Serial.println(F("=================================================="));
                    Serial.println(F(">>> [RELAY STOP - SAFETY CUTOFF]"));
                    Serial.print(F("    Reason: Humidity reached "));
                    Serial.print(h, 1);
                    Serial.println(F("% (>= 95.0% limit)"));
                    Serial.println(F("    Humidifier & Fan turned OFF to prevent oversaturation"));
                    Serial.println(F("    Status: Paused until humidity drops < 95%"));
                    Serial.println(F("=================================================="));
                    Serial.println();
                } else if (!_testBlockedByHumidity) {
                    _testBlockedByHumidity = true;
                    Serial.println(F(">>> [RELAYS REMAIN OFF] In ON window, but humidity is >= 95%"));
                }
            } else {
                // Below 95% -> relays should be ON in the OnPhase
                if (!_humidifierOn || !_fanOn) {
                    _humidifierOn = true;
                    _fanOn = true;
                    _testBlockedByHumidity = false;
                    setRelay(HUMIDIFIER_RELAY_PIN, true);
                    setRelay(FAN_RELAY_PIN, true);

                    Serial.println();
                    Serial.println(F("=================================================="));
                    Serial.println(F(">>> [RELAY START] Humidifier & Fan turned ON"));
                    Serial.print(F("    Reason: Humidity is "));
                    Serial.print(h, 1);
                    Serial.println(F("% (< 95.0% threshold OK)"));
                    Serial.print(F("    Cycle: ON window remaining: "));
                    Serial.print(testPhaseRemainingMs() / 1000);
                    Serial.println(F("s"));
                    Serial.println(F("=================================================="));
                    Serial.println();
                }
            }
        } else {
            // OffPhase: ensure relays stay OFF
            if (_humidifierOn || _fanOn) {
                _humidifierOn = false;
                _fanOn = false;
                setRelay(HUMIDIFIER_RELAY_PIN, false);
                setRelay(FAN_RELAY_PIN, false);
                Serial.println(F(">>> [RELAY STOP] Relays forced OFF (currently in scheduled OFF window)"));
            }
        }
        return;
    }

    // -------------------------------------------------------------
    // Production Mode: Threshold and safety state machine
    // Relay logic:
    // Stops immediately when humidity reaches HUMIDIFIER_OFF_ABOVE_PCT (95.0% RH).
    // Starts when humidity drops strictly below HUMIDIFIER_ON_BELOW_PCT (92.0% RH).
    // -------------------------------------------------------------
    const bool reachedStopThreshold = (h >= HUMIDIFIER_OFF_ABOVE_PCT);
    const bool belowStartThreshold  = (h < HUMIDIFIER_ON_BELOW_PCT);

    switch (_relayState) {
        case RelayState::Idle:
            if (belowStartThreshold) {
                _humidifierOn = true;
                _fanOn = false;
                _humidifierRunStartMs = millis();
                _fanTimerStartMs = millis();
                _relayState = RelayState::HumidOnFanWait;
                setRelay(HUMIDIFIER_RELAY_PIN, true);
                setRelay(FAN_RELAY_PIN, false);
                Serial.println(F("Relay: Humidity < 92% -> Humidifier ON, fan starting in 5s"));
            }
            break;

        case RelayState::HumidOnFanWait:
            if (reachedStopThreshold) {
                // Reached 95% before fan start lag elapsed; fan never starts
                _humidifierOn = false;
                _fanOn = false;
                _humidifierRunStartMs = 0;
                _relayState = RelayState::Idle;
                setRelay(HUMIDIFIER_RELAY_PIN, false);
                setRelay(FAN_RELAY_PIN, false);
                Serial.println(F("Relay: Reached 95% before fan started -> Humidifier OFF"));
            }
            break;

        case RelayState::BothOn:
            if (reachedStopThreshold) {
                // Reached 95%: humidifier turns off immediately; fan runs for lag duration to clear mist
                _humidifierOn = false;
                _fanOn = true;
                _humidifierRunStartMs = 0; // reset continuous run counter on normal cycle completion
                _enteringRest = false;
                _fanTimerStartMs = millis();
                _relayState = RelayState::FanOffWait;
                setRelay(HUMIDIFIER_RELAY_PIN, false);
                setRelay(FAN_RELAY_PIN, true);
                Serial.println(F("Relay: Reached 95% -> Humidifier OFF, fan clearing mist for 5s"));
            }
            break;

        case RelayState::FanOffWait:
            // If humidity drops below start threshold again while fan was still clearing mist
            // AND we are not in a forced 3-hour rest transition:
            if (belowStartThreshold && !_enteringRest) {
                _humidifierOn = true;
                _fanOn = true;
                _relayState = RelayState::BothOn;
                _humidifierRunStartMs = millis();
                setRelay(HUMIDIFIER_RELAY_PIN, true);
                setRelay(FAN_RELAY_PIN, true);
                Serial.println(F("Relay: Humidity dropped below 92% while fan clearing mist -> Humidifier back ON"));
            }
            break;

        case RelayState::RestCooldown:
            // Humidifier is locked in mandatory 30-min rest; ignore humidity readings
            break;
    }
}
