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
// Device identity
// ---------------------------------------------------------
#define FIRMWARE_VERSION    "1.3.0"
#define DEVICE_NAME          "Grow Room 1"
#define DEVICE_TYPE          "controller"

// ---------------------------------------------------------
// MQTT (host/port/credentials live in Preferences via SoftAP)
// ---------------------------------------------------------
#define MQTT_DEFAULT_PORT_LOCAL   1883
#define MQTT_DEFAULT_PORT_CLOUD   8883
#define MQTT_BACKEND_HEALTH_PORT  4000
#define MQTT_TEST_TIMEOUT_MS      8000
#define MQTT_CLIENT_ID            "mushroom-unit-1"
#define DEVICE_ID                 "unit1"
#define MQTT_RETRY_INTERVAL_MS    5000

#endif // CONFIG_H
