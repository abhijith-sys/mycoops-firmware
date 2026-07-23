#ifndef PROVISIONING_STORE_H
#define PROVISIONING_STORE_H

#include <Arduino.h>
#include <Preferences.h>

struct ProvisioningConfig {
    String wifiSsid;
    String wifiPassword;
    String mqttHost;
    uint16_t mqttPort;
    String mqttUser;
    String mqttPass;
    bool mqttTls;
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
                         const String &pass, bool tls, const String &mode);

private:
    static constexpr const char *NAMESPACE = "wifi";
};

#endif // PROVISIONING_STORE_H
