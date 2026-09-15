#ifndef STATUS_OUTPUTS_H
#define STATUS_OUTPUTS_H

#include "Sensor.h"

// Drives green/red status LEDs and 2-channel relay (humidifier + fan).
class StatusOutputs {
public:
    enum class RelayState {
        Idle,           // Both relays OFF
        HumidOnFanWait, // Humidifier ON, waiting for fan lag before turning fan ON
        BothOn,         // Both humidifier and fan ON
        FanOffWait,     // Humidifier OFF, fan stays ON clearing mist for lag duration
        RestCooldown    // Humidifier and fan OFF; mandatory 30-min rest cooldown active
    };

    enum class TestCyclePhase {
        OnPhase,        // In scheduled ON window (relays ON unless blocked by >=95% humidity)
        OffPhase        // In scheduled OFF window (both relays OFF)
    };

    void begin();

    // Advances fan lag timers, run limit, and rest cooldown timers non-blockingly. Call every loop() iteration.
    void loop();

    // Updates LEDs and relay state machine based on sensor reading.
    void update(const SensorReading &reading, float targetTempC, float targetHumidityPct);

    // True when humidity is below the ideal LED band.
    bool needsHumidifier() const { return _needsHumidifier; }

    // True when temperature is above the ideal LED band.
    bool needsCooling() const { return _needsCooling; }

    // Live relay statuses
    bool isHumidifierOn() const { return _humidifierOn; }
    bool isFanOn() const { return _fanOn; }
    bool isResting() const { return _relayState == RelayState::RestCooldown; }
    RelayState relayState() const { return _relayState; }

    unsigned long restRemainingMs() const;
    unsigned long continuousRunMs() const;

    // Test cycle mode controls and status (30min ON / 30min OFF alternating)
    bool isTestCycleMode() const { return _testCycleMode; }
    void setTestCycleMode(bool enabled);
    void setTestIntervals(unsigned long onMs, unsigned long offMs);
    unsigned long testOnDurationMs() const { return _testOnDurationMs; }
    unsigned long testOffDurationMs() const { return _testOffDurationMs; }
    TestCyclePhase testPhase() const { return _testPhase; }
    unsigned long testPhaseRemainingMs() const;
    bool isTestBlockedByHumidity() const { return _testBlockedByHumidity; }

private:
    void setRelay(uint8_t pin, bool on);

    bool _needsHumidifier;
    bool _needsCooling;

    // Production threshold state machine
    RelayState _relayState;
    unsigned long _fanTimerStartMs;
    unsigned long _humidifierRunStartMs;
    unsigned long _restTimerStartMs;
    bool _enteringRest;

    // Test cycle mode state
    bool _testCycleMode;
    unsigned long _testOnDurationMs;
    unsigned long _testOffDurationMs;
    TestCyclePhase _testPhase;
    unsigned long _testPhaseStartMs;
    bool _testBlockedByHumidity;
    float _lastHumidity;
    bool _lastReadingValid;
    uint8_t _consecutiveFailedReads;

    // Current output pin levels
    bool _humidifierOn;
    bool _fanOn;
};

#endif // STATUS_OUTPUTS_H
