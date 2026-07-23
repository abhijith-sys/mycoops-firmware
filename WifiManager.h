#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

class WifiManager {
public:
    WifiManager();

    // Blocking connect attempt, up to WIFI_CONNECT_TIMEOUT_MS.
    // Returns true if connected before the timeout.
    bool begin();

    bool isConnected();

    // Call every loop(). Non-blocking - retries in the background
    // if the connection was lost, without spamming reconnect attempts.
    void ensureConnected();

private:
    unsigned long _lastReconnectAttempt;
};

#endif // WIFI_MANAGER_H