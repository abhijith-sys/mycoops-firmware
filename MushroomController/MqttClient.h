#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "Config.h"
#include <WiFiClient.h>
#include <PubSubClient.h>
#if MQTT_ENABLE_CLOUD_WSS
#include "mqtt_client.h"
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
    void startEspMqtt();
    void stopEspMqtt();
    static void espMqttEventThunk(void *handler_args, esp_event_base_t base,
                                  int32_t event_id, void *event_data);
    void onEspMqttEvent(int32_t event_id, esp_mqtt_event_handle_t event);
#endif

    WiFiClient   _wifiClient;
    PubSubClient _mqttClient;
#if MQTT_ENABLE_CLOUD_WSS
    esp_mqtt_client_handle_t _espClient;
    bool                     _espStarted;
    bool                     _espConnected;
    String                   _wssUri;  // must outlive esp_mqtt_client config pointers
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
