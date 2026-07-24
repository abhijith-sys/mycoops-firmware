#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "Config.h"
#include <WiFiClient.h>
#if MQTT_ENABLE_TLS
#include <WiFiClientSecure.h>
#endif
#include <PubSubClient.h>
#include "Sensor.h"

class MqttClient {
public:
    MqttClient();

    // Loads MQTT settings from Preferences. Connection is lazy in ensureConnected().
    bool begin();

    void ensureConnected();
    bool isConnected();

    bool publishReading(const SensorReading &reading, float targetTemp, float targetHumidity);

private:
    void applyTransport();

    WiFiClient       _wifiClient;
#if MQTT_ENABLE_TLS
    WiFiClientSecure _secureClient;
#endif
    PubSubClient     _mqttClient;
    unsigned long    _lastReconnectAttempt;

    String   _host;
    uint16_t _port;
    String   _user;
    String   _pass;
    bool     _tls;
    bool     _configured;

    String _sensorTopic;
    String _statusTopic;
};

#endif // MQTT_CLIENT_H
