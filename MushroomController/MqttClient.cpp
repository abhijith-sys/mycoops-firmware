#include "MqttClient.h"
#include "Topics.h"
#include "DeviceInfo.h"
#include "ProvisioningStore.h"
#include <ArduinoJson.h>

MqttClient::MqttClient()
    : _mqttClient(_wifiClient),
#if MQTT_ENABLE_CLOUD_WSS
      _psychicStarted(false),
#endif
      _lastReconnectAttempt(0),
      _port(0),
      _tls(false),
      _configured(false),
      _willMessage("{\"online\":false}") {
}

void MqttClient::applyLocalTransport() {
    _mqttClient.setClient(_wifiClient);
    _mqttClient.setServer(_host.c_str(), _port);
}

#if MQTT_ENABLE_CLOUD_WSS
String MqttClient::buildWssUri() const {
    String path = _path.length() ? _path : String(MQTT_DEFAULT_WS_PATH);
    if (!path.startsWith("/")) {
        path = String("/") + path;
    }
    return String("wss://") + _host + ":" + String(_port) + path;
}

void MqttClient::configureCloudWss() {
    _wssUri = buildWssUri();
    _psychic.setServer(_wssUri.c_str());
    _psychic.setClientId(MQTT_CLIENT_ID);
    _psychic.setKeepAlive(60);
    _psychic.setCleanSession(true);
    _psychic.setAutoReconnect(true);
    _psychic.setBufferSize(1024);
    // Validate against Arduino CA bundle (Cloudflare / Let's Encrypt).
    _psychic.attachArduinoCACertBundle(true);
    if (_user.length() > 0) {
        _psychic.setCredentials(_user.c_str(), _pass.length() ? _pass.c_str() : nullptr);
    } else {
        _psychic.setCredentials(nullptr, nullptr);
    }
    _psychic.setWill(_statusTopic.c_str(), 1, true, _willMessage.c_str());
}
#endif

bool MqttClient::begin() {
    ProvisioningConfig cfg;
    ProvisioningStore::load(cfg);

    _host = cfg.mqttHost;
    _port = cfg.mqttPort;
    _user = cfg.mqttUser;
    _pass = cfg.mqttPass;
    _path = cfg.mqttPath.length() ? cfg.mqttPath : String(MQTT_DEFAULT_WS_PATH);
    _tls  = cfg.mqttTls || cfg.isCloud();
    _configured = cfg.hasMqtt();

#if !MQTT_ENABLE_CLOUD_WSS
    if (_tls) {
        Serial.println(F("MQTT cloud/WSS requested but MQTT_ENABLE_CLOUD_WSS=0 — using plain MQTT. Rebuild with MQTT_ENABLE_CLOUD_WSS 1 for Cloudflare."));
        _tls = false;
    }
#endif

    String prefix = String(MQTT_TOPIC_PREFIX) + DEVICE_ID;
    _sensorTopic = prefix + MQTT_TOPIC_SENSORS_SUFFIX;
    _statusTopic = prefix + MQTT_TOPIC_STATUS_SUFFIX;

    if (!_configured) {
        Serial.println(F("MQTT not provisioned yet (no mqtt_host in Preferences)"));
        return false;
    }

#if MQTT_ENABLE_CLOUD_WSS
    if (_tls) {
        configureCloudWss();
        Serial.print(F("MQTT configured → "));
        Serial.print(buildWssUri());
        Serial.println(F(" (WSS)"));
        return true;
    }
#endif

    applyLocalTransport();
    Serial.print(F("MQTT configured → "));
    Serial.print(_host);
    Serial.print(F(":"));
    Serial.println(_port);
    return true;
}

bool MqttClient::isConnected() {
    if (!_configured) {
        return false;
    }
#if MQTT_ENABLE_CLOUD_WSS
    if (_tls) {
        return _psychic.connected();
    }
#endif
    return _mqttClient.connected();
}

void MqttClient::ensureConnected() {
    if (!_configured) {
        return;
    }

#if MQTT_ENABLE_CLOUD_WSS
    if (_tls) {
        if (!_psychicStarted) {
            unsigned long now = millis();
            if (now - _lastReconnectAttempt < MQTT_RETRY_INTERVAL_MS && _lastReconnectAttempt != 0) {
                return;
            }
            _lastReconnectAttempt = now;
            configureCloudWss();
            _psychic.onConnect([this](bool) {
                Serial.print(F("MQTT connected to "));
                Serial.println(buildWssUri());
                _psychic.publish(_statusTopic.c_str(), 1, true, "{\"online\":true}");
            });
            _psychic.onError([](esp_mqtt_error_codes_t) {
                Serial.println(F("MQTT WSS error (broker down, TLS, or tunnel?)"));
            });
            _psychic.connect();
            _psychicStarted = true;
            Serial.print(F("MQTT WSS connecting → "));
            Serial.println(buildWssUri());
        }
        return;
    }
#endif

    if (_mqttClient.connected()) {
        _mqttClient.loop();
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < MQTT_RETRY_INTERVAL_MS) {
        return;
    }
    _lastReconnectAttempt = now;

    applyLocalTransport();

    const char *willMessage = _willMessage.c_str();
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
    if (!isConnected()) {
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

#if MQTT_ENABLE_CLOUD_WSS
    if (_tls) {
        return _psychic.publish(_sensorTopic.c_str(), 0, false, payload, (int)len) >= 0;
    }
#endif
    return _mqttClient.publish(_sensorTopic.c_str(), payload, len);
}
