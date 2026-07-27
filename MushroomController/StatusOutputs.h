#ifndef STATUS_OUTPUTS_H
#define STATUS_OUTPUTS_H

#include "Sensor.h"

// Drives green/red status LEDs from live climate vs targets.
// Later the same "needs humidity" / "too hot" decisions can drive relays.
class StatusOutputs {
public:
    void begin();

    // Updates LEDs from a sensor reading and ideal targets.
    void update(const SensorReading &reading, float targetTempC, float targetHumidityPct);

    // True when humidity is below the ideal band (future: turn humidifier ON).
    bool needsHumidifier() const { return _needsHumidifier; }

    // True when temperature is above the ideal band (future: cooler / exhaust).
    bool needsCooling() const { return _needsCooling; }

private:
    bool _needsHumidifier;
    bool _needsCooling;
};

#endif // STATUS_OUTPUTS_H
