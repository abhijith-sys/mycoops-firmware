#include "MqttClient.h"
#include "Config.h"
#include "Topics.h"
#include "DeviceInfo.h"
#include "ProvisioningStore.h"
#include <ArduinoJson.h>

MqttClient::MqttClient()
    : _mqttClient(_wifiClient),
      _lastReconnectAttempt(0),
      _port(0),
      _tls(false),
      _configured(false) {
}

void MqttClient::applyTransport() {
    if (_tls) {
        _secureClient.setInsecure();  // CA pinning is a follow-up hardening step
        _mqttClient.setClient(_secureClient);
    } else {
        _mqttClient.setClient(_wifiClient);
    }
    _mqttClient.setServer(_host.c_str(), _port);
}

bool MqttClient::begin() {
    ProvisioningConfig cfg;
    ProvisioningStore::load(cfg);

    _host = cfg.mqttHost;
    _port = cfg.mqttPort;
    _user = cfg.mqttUser;
    _pass = cfg.mqttPass;
    _tls  = cfg.mqttTls;
    _configured = cfg.hasMqtt();

    String prefix = String(MQTT_TOPIC_PREFIX) + DEVICE_ID;
    _sensorTopic = prefix + MQTT_TOPIC_SENSORS_SUFFIX;
    _statusTopic = prefix + MQTT_TOPIC_STATUS_SUFFIX;

    if (!_configured) {
        Serial.println(F("MQTT not provisioned yet (no mqtt_host in Preferences)"));
        return false;
    }

    applyTransport();
    Serial.print(F("MQTT configured → "));
    Serial.print(_host);
    Serial.print(F(":"));
    Serial.print(_port);
    Serial.println(_tls ? F(" (TLS)") : F(""));
    return true;
}

bool MqttClient::isConnected() {
    return _configured && _mqttClient.connected();
}

void MqttClient::ensureConnected() {
    if (!_configured) {
        return;
    }

    if (_mqttClient.connected()) {
        _mqttClient.loop();
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < MQTT_RETRY_INTERVAL_MS) {
        return;
    }
    _lastReconnectAttempt = now;

    applyTransport();

    const char *willMessage = "{\"online\":false}";
    bool connected = false;
    if (_user.length() > 0) {
        connected = _mqttClient.connect(
            MQTT_CLIENT_ID,
            _user.c_str(),
            _pass.c_str(),
            _statusTopic.c_str(),
            1,
            true,
            willMessage);
    } else {
        connected = _mqttClient.connect(
            MQTT_CLIENT_ID,
            _statusTopic.c_str(),
            1,
            true,
            willMessage);
    }

    if (connected) {
        Serial.print(F("MQTT connected to "));
        Serial.print(_host);
        Serial.print(F(":"));
        Serial.println(_port);
        _mqttClient.publish(_statusTopic.c_str(), "{\"online\":true}", true);
    } else {
        Serial.print(F("MQTT connect FAILED → "));
        Serial.print(_host);
        Serial.print(F(":"));
        Serial.print(_port);
        Serial.println(F(" (broker down, wrong host, or firewall?)"));
    }
}

bool MqttClient::publishReading(const SensorReading &reading, float targetTemp, float targetHumidity) {
    if (!_mqttClient.connected()) {
        return false;
    }

    StaticJsonDocument<384> doc;

    DeviceInfo::fill(doc);
    reading.fill(doc);

    doc["target_temperature"] = targetTemp;
    doc["target_humidity"]    = targetHumidity;
    doc["uptime_ms"]          = millis();

    char payload[384];
    size_t len = serializeJson(doc, payload);

    return _mqttClient.publish(_sensorTopic.c_str(), payload, len);
}
