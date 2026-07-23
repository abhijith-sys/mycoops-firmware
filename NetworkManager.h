#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

class NetworkManager {
public:
    NetworkManager();

    // Load credentials, connect STA or enter SoftAP setup portal.
    void begin();

    // Call every loop(). Serves the portal in setup mode, or
    // non-blocking STA reconnect (fail-into-setup after N attempts).
    void loop();

    bool isConnected();
    bool isSetupMode();

private:
    Preferences _prefs;
    WebServer   _server;
    DNSServer   _dns;

    bool          _setupMode;
    bool          _reconnectInProgress;
    bool          _handlersRegistered;
    String        _ssid;
    String        _password;
    unsigned long _lastReconnectAttempt;
    int           _failedAttempts;
    String        _portalMessage;  // shown on portal after save attempt

    void loadCredentials();
    void saveCredentials(const String &ssid, const String &password);
    bool hasCredentials() const;

    bool connectSta(unsigned long timeoutMs);
    void tryReconnect();
    void enterSetupMode();

    String softApSsid();
    void   startPortal();
    void   stopPortal();

    void handleRoot();
    void handleSave();
    void handleNotFound();

    String buildPortalHtml();
};

#endif // NETWORK_MANAGER_H
