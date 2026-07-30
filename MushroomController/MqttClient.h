#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "Config.h"
#include <WiFiClient.h>
#include <PubSubClient.h>
#if MQTT_ENABLE_CLOUD_WSS
#include <PsychicMqttClient.h>
#endif
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
    void applyLocalTransport();
#if MQTT_ENABLE_CLOUD_WSS
    void configureCloudWss();
    String buildWssUri() const;
#endif

    WiFiClient   _wifiClient;
    PubSubClient _mqttClient;
#if MQTT_ENABLE_CLOUD_WSS
    PsychicMqttClient _psychic;
    bool              _psychicStarted;
    String            _wssUri;  // must outlive Psychic setServer(c_str)
#endif
    unsigned long _lastReconnectAttempt;

    String   _host;
    uint16_t _port;
    String   _user;
    String   _pass;
    String   _path;
    bool     _tls;   // cloud WSS when true
    bool     _configured;

    String _sensorTopic;
    String _statusTopic;
    String _willMessage;
};

#endif // MQTT_CLIENT_H
