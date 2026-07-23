#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
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
    WiFiClientSecure _secureClient;
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
