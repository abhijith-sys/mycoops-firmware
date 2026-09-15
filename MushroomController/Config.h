#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------
// I2C Pins (ESP32)
// ---------------------------------------------------------
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22

// ---------------------------------------------------------
// OLED Display (SSD1306)
// ---------------------------------------------------------
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_ADDR           0x3C

// ---------------------------------------------------------
// SHT31 Temperature / Humidity Sensor
// ---------------------------------------------------------
#define SHT31_ADDR          0x44

// ---------------------------------------------------------
// Target conditions for the current growth stage
// ---------------------------------------------------------
#define TARGET_TEMPERATURE  28.0f   // deg C
#define TARGET_HUMIDITY     90.0f   // % RH

// How far from target still counts as "ideal" (green).
#define CLIMATE_TEMP_TOLERANCE_C   1.0f
#define CLIMATE_HUM_TOLERANCE_PCT  5.0f

// ---------------------------------------------------------
// Status LEDs — 4 total (active HIGH → resistor → LED → GND)
// Temp:  green GPIO12 / red GPIO13
// Hum:   green GPIO14 / red GPIO15
// Ideal band = target ± tolerance below. Out of range → that pair's red ON.
// Note: GPIO 12 is a strapping pin — do not pull it HIGH at boot.
// ---------------------------------------------------------
#define TEMP_LED_GREEN_PIN  12
#define TEMP_LED_RED_PIN    13
#define HUM_LED_GREEN_PIN   14
#define HUM_LED_RED_PIN     15

// ---------------------------------------------------------
// 2-channel relay — humidifier (IN1) + fan (IN2)
// Do NOT reuse LED pins (12–15) or I2C (21/22). Never mains on ESP GPIO.
// Typical modules are active LOW (IN LOW = coil ON). Set RELAY_ACTIVE_LOW 0
// if your board is active HIGH.
// ---------------------------------------------------------
#define HUMIDIFIER_RELAY_PIN          25
#define FAN_RELAY_PIN                 26
#define RELAY_ACTIVE_LOW              1

// Humidity thresholds (% RH)
// Humidifier starts when humidity drops strictly below HUMIDIFIER_ON_BELOW_PCT (92.0%).
// Humidifier stops immediately when humidity reaches HUMIDIFIER_OFF_ABOVE_PCT (95.0%).
#define HUMIDIFIER_ON_BELOW_PCT       92.0f
#define HUMIDIFIER_OFF_ABOVE_PCT      95.0f

// Fan starts this many ms after humidifier ON, and stops this many ms
// after humidifier OFF (while humidifier stays off). Tunable for efficiency.
#define FAN_LAG_AFTER_HUMIDIFIER_MS   5000

// Humidifier protection: maximum continuous run time (3 hours).
// If humidity does not reach 95% within this period, humidifier force-stops.
#define HUMIDIFIER_MAX_CONTINUOUS_RUN_MS  (3UL * 60UL * 60UL * 1000UL) // 3 hours

// Mandatory rest cooldown (30 minutes) after hitting max continuous run time.
// Humidifier cannot start again until this rest period elapses.
#define HUMIDIFIER_REST_INTERVAL_MS       (30UL * 60UL * 1000UL)        // 30 minutes

// ---------------------------------------------------------
// 2-Channel Relay Test Cycle Mode (Testing vs Production)
// When true: Alternates both Humidifier & Fan ON for RELAY_TEST_ON_DURATION_MS,
//            then OFF for RELAY_TEST_OFF_DURATION_MS.
//            During ON phase: if humidity is >= 95% (HUMIDIFIER_OFF_ABOVE_PCT),
//            relays will NOT start (or will turn OFF if already running).
// When false: Standard production threshold mode (starts <92%, stops >=95%,
//             5s fan lag, 3-hour continuous run limit, 30-min rest cooldown).
// ---------------------------------------------------------
#define RELAY_TEST_CYCLE_MODE             true

// Test cycle durations (tunable variables - change for testing, e.g. 10000 for 10s):
#define RELAY_TEST_ON_DURATION_MS         (30UL * 60UL * 1000UL)        // 30 minutes ON
#define RELAY_TEST_OFF_DURATION_MS        (30UL * 60UL * 1000UL)        // 30 minutes OFF

// ---------------------------------------------------------
// Timing
// ---------------------------------------------------------
#define SENSOR_READ_INTERVAL_MS   2000
#define SERIAL_BAUD_RATE          115200

// ---------------------------------------------------------
// WiFi (credentials live in Preferences — not hardcoded)
// ---------------------------------------------------------
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RETRY_INTERVAL_MS    5000
#define WIFI_MAX_FAILED_ATTEMPTS  5
#define WIFI_SETUP_AP_PREFIX      "GrowOS-Setup-"

// ---------------------------------------------------------
// BLE local sensor (Chrome Web Bluetooth)
// ---------------------------------------------------------
#define BLE_DEVICE_NAME_PREFIX  "GrowOS-"
#define BLE_SERVICE_UUID        "6b6a0001-7c7a-4f3e-9b2d-1e5f8a9c0d01"
#define BLE_CHAR_UUID           "6b6a0002-7c7a-4f3e-9b2d-1e5f8a9c0d01"

// ---------------------------------------------------------
// Device identity
// ---------------------------------------------------------
#define FIRMWARE_VERSION    "1.6.0"
#define DEVICE_NAME          "Grow Room 1"
#define DEVICE_TYPE          "controller"

// ---------------------------------------------------------
// MQTT (host/port/credentials live in Preferences via SoftAP)
// ---------------------------------------------------------
// Cloud mode uses MQTT over WebSocket Secure (WSS) via ESP-IDF esp_mqtt
// (built into ESP32 Arduino core — no extra Library Manager package).
// Local Mosquitto on 1883 works with PubSubClient when cloud is off.
#ifndef MQTT_ENABLE_CLOUD_WSS
#ifdef MQTT_ENABLE_TLS
#define MQTT_ENABLE_CLOUD_WSS     MQTT_ENABLE_TLS
#else
#define MQTT_ENABLE_CLOUD_WSS     1
#endif
#endif
#define MQTT_DEFAULT_PORT_LOCAL   1883
#define MQTT_DEFAULT_PORT_CLOUD   443
#define MQTT_DEFAULT_WS_PATH      "/mqtt"
#define MQTT_BACKEND_HEALTH_PORT  4000
#define MQTT_TEST_TIMEOUT_MS      8000
#define MQTT_CLIENT_ID            "mushroom-unit-1"
#define DEVICE_ID                 "unit1"
#define MQTT_RETRY_INTERVAL_MS    5000

#endif // CONFIG_H
