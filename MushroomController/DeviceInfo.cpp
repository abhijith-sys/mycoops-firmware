#include "DeviceInfo.h"
#include "Config.h"
#include <WiFi.h>

namespace DeviceInfo {
    void fill(JsonDocument &doc) {
        doc["device_id"]   = DEVICE_ID;
        doc["device_name"] = DEVICE_NAME;
        doc["device_type"] = DEVICE_TYPE;
        doc["firmware"]    = FIRMWARE_VERSION;
        doc["ip"]          = WiFi.localIP().toString();
        doc["wifi_rssi"]   = WiFi.RSSI();
    }
}