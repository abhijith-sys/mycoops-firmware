#ifndef GROW_NETWORK_MANAGER_H
#define GROW_NETWORK_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ProvisioningStore.h"

// Named GrowNetworkManager to avoid clashing with ESP32 Arduino Core 3.x
// NetworkManager (built-in).
class GrowNetworkManager {
public:
    GrowNetworkManager();

    void begin();
    void loop();

    bool isConnected();
    bool isSetupMode();

    String setupApSsid() const;
    int    softApClientCount() const;

private:
    WebServer _server;
    DNSServer _dns;

    ProvisioningConfig _cfg;
    bool          _setupMode;
    bool          _reconnectInProgress;
    bool          _handlersRegistered;
    bool          _mqttTestOk;
    int           _setupStep;  // 1 = WiFi, 2 = MQTT
    String        _apSsid;
    unsigned long _lastReconnectAttempt;
    unsigned long _lastSetupStatusPrint;
    int           _failedAttempts;
    String        _portalMessage;
    String        _portalOkMessage;

    bool connectSta(unsigned long timeoutMs, bool keepAp);
    void tryReconnect();
    void enterSetupMode(int step);

    String softApSsid();
    void   startPortal(bool keepSta);
    void   stopPortal();

    void handleRoot();
    void handleSaveWifi();
    void handleSuggest();
    void handleTestMqtt();
    void handleTestBackend();
    void handleSaveMqtt();
    void handleNotFound();

    String buildWifiPortalHtml();
    String buildMqttPortalHtml();
    String htmlEscape(const String &in);
    bool   probeBackendHealth(const String &host, uint16_t port, String &errOut);
    bool   probeMqttConnect(const String &host, uint16_t port, const String &user,
                            const String &pass, bool tls, String &errOut);
};

#endif // GROW_NETWORK_MANAGER_H
