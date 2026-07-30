#ifndef PROVISIONING_STORE_H
#define PROVISIONING_STORE_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

struct ProvisioningConfig {
    String wifiSsid;
    String wifiPassword;
    String mqttHost;
    uint16_t mqttPort;
    String mqttUser;
    String mqttPass;
    String mqttPath;  // WSS path, e.g. "/mqtt" (cloud only)
    bool mqttTls;     // true for cloud WSS
    String mqttMode;  // "local" or "cloud"

    bool hasWifi() const { return wifiSsid.length() > 0; }
    bool hasMqtt() const { return mqttHost.length() > 0 && mqttPort > 0; }
    bool isCloud() const { return mqttMode == "cloud" || mqttTls; }
};

class ProvisioningStore {
public:
    static void load(ProvisioningConfig &out);
    static void saveWifi(const String &ssid, const String &password);
    static void saveMqtt(const String &host, uint16_t port, const String &user,
                         const String &pass, bool tls, const String &mode,
                         const String &path = MQTT_DEFAULT_WS_PATH);

private:
    static constexpr const char *NAMESPACE = "wifi";
};

#endif // PROVISIONING_STORE_H
