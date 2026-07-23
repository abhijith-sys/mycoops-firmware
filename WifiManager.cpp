#include "WifiManager.h"
#include "Config.h"

WifiManager::WifiManager() : _lastReconnectAttempt(0) {
}

bool WifiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }

    return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::ensureConnected() {
    if (isConnected()) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < WIFI_RETRY_INTERVAL_MS) {
        return; // avoid hammering reconnect attempts
    }
    _lastReconnectAttempt = now;

    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}