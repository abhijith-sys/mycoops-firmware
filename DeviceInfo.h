#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <ArduinoJson.h>

// Adds device identity + health fields (firmware, ip, wifi signal, etc.)
// to any JSON document. Keeps this knowledge out of MqttClient so it
// doesn't need to know what a "device" is.
namespace DeviceInfo {
    void fill(JsonDocument &doc);
}

#endif // DEVICE_INFO_H