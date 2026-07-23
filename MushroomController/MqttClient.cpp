#include "MqttClient.h"
#include "Config.h"
#include "Topics.h"
#include "DeviceInfo.h"
#include <ArduinoJson.h>

MqttClient::MqttClient() : _mqttClient(_wifiClient), _lastReconnectAttempt(0) {
}

bool MqttClient::begin() {
    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

    String prefix = String(MQTT_TOPIC_PREFIX) + DEVICE_ID;
    _sensorTopic = prefix + MQTT_TOPIC_SENSORS_SUFFIX;
    _statusTopic = prefix + MQTT_TOPIC_STATUS_SUFFIX;

    return true;
}

bool MqttClient::isConnected() {
    return _mqttClient.connected();
}

void MqttClient::ensureConnected() {
    if (_mqttClient.connected()) {
        _mqttClient.loop();
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < MQTT_RETRY_INTERVAL_MS) {
        return; // avoid hammering reconnect attempts
    }
    _lastReconnectAttempt = now;

    // Last Will: if this device drops off ungracefully (power loss, crash,
    // WiFi death), the broker publishes this on our behalf so the dashboard
    // shows "offline" instead of silently going stale.
    const char *willMessage = "{\"online\":false}";
    bool connected = _mqttClient.connect(
        MQTT_CLIENT_ID,
        _statusTopic.c_str(),
        1,            // will QoS
        true,         // will retained
        willMessage
    );

    if (connected) {
        // Birth message: retained, so any dashboard that connects later
        // immediately sees the current online state without waiting for
        // the next sensor publish.
        _mqttClient.publish(_statusTopic.c_str(), "{\"online\":true}", true);
    }
}

bool MqttClient::publishReading(const SensorReading &reading, float targetTemp, float targetHumidity) {
    if (!_mqttClient.connected()) {
        return false;
    }

    StaticJsonDocument<384> doc;

    DeviceInfo::fill(doc);
    reading.fill(doc);

    // Send the target itself, not a precomputed delta - if the target
    // changes later, historical rows still make sense on their own.
    doc["target_temperature"] = targetTemp;
    doc["target_humidity"]    = targetHumidity;

    doc["uptime_ms"] = millis();

    char payload[384];
    size_t len = serializeJson(doc, payload);

    return _mqttClient.publish(_sensorTopic.c_str(), payload, len);
}