#include "ProvisioningStore.h"
#include "Config.h"

void ProvisioningStore::load(ProvisioningConfig &out) {
    Preferences prefs;
    prefs.begin(NAMESPACE, true);
    out.wifiSsid     = prefs.getString("wifi_ssid", "");
    out.wifiPassword = prefs.getString("wifi_password", "");
    out.mqttHost     = prefs.getString("mqtt_host", "");
    out.mqttPort     = (uint16_t)prefs.getUShort("mqtt_port", 0);
    out.mqttUser     = prefs.getString("mqtt_user", "");
    out.mqttPass     = prefs.getString("mqtt_pass", "");
    out.mqttTls      = prefs.getBool("mqtt_tls", false);
    out.mqttMode     = prefs.getString("mqtt_mode", "local");
    prefs.end();

    if (out.mqttPort == 0 && out.mqttHost.length() > 0) {
        out.mqttPort = out.mqttTls ? MQTT_DEFAULT_PORT_CLOUD : MQTT_DEFAULT_PORT_LOCAL;
    }
    if (out.mqttMode.length() == 0) {
        out.mqttMode = out.mqttTls ? "cloud" : "local";
    }
}

void ProvisioningStore::saveWifi(const String &ssid, const String &password) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_password", password);
    prefs.end();
}

void ProvisioningStore::saveMqtt(const String &host, uint16_t port, const String &user,
                                 const String &pass, bool tls, const String &mode) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    prefs.putString("mqtt_host", host);
    prefs.putUShort("mqtt_port", port);
    prefs.putString("mqtt_user", user);
    prefs.putString("mqtt_pass", pass);
    prefs.putBool("mqtt_tls", tls);
    prefs.putString("mqtt_mode", mode);
    prefs.end();
}
