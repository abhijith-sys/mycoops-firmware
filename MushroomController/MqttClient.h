#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <WiFiClient.h>
#include <PubSubClient.h>
#include "Sensor.h"

class MqttClient {
public:
    MqttClient();

    // Configures the broker address/port and builds this device's topic
    // strings. Actual connection happens lazily in ensureConnected()
    // since WiFi may not be up yet.
    bool begin();

    // Call every loop(). Non-blocking reconnect if the broker connection
    // dropped. On (re)connect, publishes a retained "online:true" birth
    // message and registers a "online:false" last-will message that the
    // broker auto-publishes if this device disconnects unexpectedly.
    void ensureConnected();

    bool isConnected();

    // Publishes one JSON sensor reading with raw values, the *target*
    // conditions (not a precomputed delta - see README), and device
    // identity/health fields (firmware, ip, rssi, etc.).
    // Returns false if not currently connected to the broker.
    bool publishReading(const SensorReading &reading, float targetTemp, float targetHumidity);

private:
    WiFiClient    _wifiClient;
    PubSubClient  _mqttClient;
    unsigned long _lastReconnectAttempt;
    String        _sensorTopic;
    String        _statusTopic;
};

#endif // MQTT_CLIENT_H