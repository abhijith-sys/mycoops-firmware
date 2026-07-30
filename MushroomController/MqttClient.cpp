#include "MqttClient.h"
#include "Topics.h"
#include "DeviceInfo.h"
#include "ProvisioningStore.h"
#include <ArduinoJson.h>
#if MQTT_ENABLE_CLOUD_WSS
#include "esp_crt_bundle.h"
#endif

MqttClient::MqttClient()
    : _mqttClient(_wifiClient),
#if MQTT_ENABLE_CLOUD_WSS
      _espClient(nullptr),
      _espStarted(false),
      _espConnected(false),
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

void MqttClient::espMqttEventThunk(void *handler_args, esp_event_base_t /*base*/,
                                   int32_t event_id, void *event_data) {
    auto *self = static_cast<MqttClient *>(handler_args);
    self->onEspMqttEvent(event_id, static_cast<esp_mqtt_event_handle_t>(event_data));
}

void MqttClient::onEspMqttEvent(int32_t event_id, esp_mqtt_event_handle_t event) {
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        _espConnected = true;
        Serial.print(F("MQTT connected to "));
        Serial.println(_wssUri);
        esp_mqtt_client_publish(_espClient, _statusTopic.c_str(), "{\"online\":true}", 0, 1, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        _espConnected = false;
        Serial.println(F("MQTT WSS disconnected"));
        break;
    case MQTT_EVENT_ERROR:
        _espConnected = false;
        Serial.println(F("MQTT WSS error (broker down, TLS, or tunnel?)"));
        break;
    default:
        break;
    }
}

void MqttClient::configureCloudWss() {
    _wssUri = buildWssUri();
}

void MqttClient::stopEspMqtt() {
    if (_espClient) {
        esp_mqtt_client_stop(_espClient);
        esp_mqtt_client_destroy(_espClient);
        _espClient = nullptr;
    }
    _espStarted = false;
    _espConnected = false;
}

void MqttClient::startEspMqtt() {
    stopEspMqtt();
    configureCloudWss();

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = _wssUri.c_str();
    // Arduino / ESP-IDF CA bundle (Cloudflare, Let's Encrypt, etc.)
    cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.credentials.client_id = MQTT_CLIENT_ID;
    if (_user.length() > 0) {
        cfg.credentials.username = _user.c_str();
        if (_pass.length() > 0) {
            cfg.credentials.authentication.password = _pass.c_str();
        }
    }
    cfg.session.keepalive = 60;
    cfg.session.disable_clean_session = false;
    cfg.session.last_will.topic = _statusTopic.c_str();
    cfg.session.last_will.msg = _willMessage.c_str();
    cfg.session.last_will.msg_len = _willMessage.length();
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = true;
    cfg.network.disable_auto_reconnect = false;
    cfg.buffer.size = 1024;

    _espClient = esp_mqtt_client_init(&cfg);
    if (!_espClient) {
        Serial.println(F("MQTT WSS init failed"));
        return;
    }
    esp_mqtt_client_register_event(_espClient, MQTT_EVENT_ANY, espMqttEventThunk, this);
    esp_err_t err = esp_mqtt_client_start(_espClient);
    if (err != ESP_OK) {
        Serial.print(F("MQTT WSS start failed: "));
        Serial.println((int)err);
        stopEspMqtt();
        return;
    }
    _espStarted = true;
    Serial.print(F("MQTT WSS connecting → "));
    Serial.println(_wssUri);
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
        Serial.print(_wssUri);
        Serial.println(F(" (WSS / esp_mqtt)"));
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
        return _espConnected;
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
        if (!_espStarted) {
            unsigned long now = millis();
            if (now - _lastReconnectAttempt < MQTT_RETRY_INTERVAL_MS && _lastReconnectAttempt != 0) {
                return;
            }
            _lastReconnectAttempt = now;
            startEspMqtt();
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
        return esp_mqtt_client_publish(_espClient, _sensorTopic.c_str(), payload, (int)len, 0, 0) >= 0;
    }
#endif
    return _mqttClient.publish(_sensorTopic.c_str(), payload, len);
}
