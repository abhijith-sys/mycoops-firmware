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
// (used to calculate the +/- delta shown on screen)
// Default values below are typical for oyster mushroom fruiting.
// Change these as you move between incubation / fruiting stages.
// ---------------------------------------------------------
#define TARGET_TEMPERATURE  28.0f   // deg C
#define TARGET_HUMIDITY     90.0f   // % RH

// ---------------------------------------------------------
// Timing
// ---------------------------------------------------------
#define SENSOR_READ_INTERVAL_MS   2000
#define SERIAL_BAUD_RATE          115200

// ---------------------------------------------------------
// WiFi
// ---------------------------------------------------------
#define WIFI_SSID                 "your-wifi-ssid"
#define WIFI_PASSWORD             "your-wifi-password"
#define WIFI_CONNECT_TIMEOUT_MS   15000
#define WIFI_RETRY_INTERVAL_MS    5000

// ---------------------------------------------------------
// Device identity
// ---------------------------------------------------------
#define FIRMWARE_VERSION    "1.1.0"
#define DEVICE_NAME          "Grow Room 1"
#define DEVICE_TYPE          "controller"

// ---------------------------------------------------------
// MQTT
// ---------------------------------------------------------
// Point this at your Mosquitto broker (e.g. Raspberry Pi IP) or a
// managed broker (HiveMQ Cloud, EMQX Cloud, etc.)
#define MQTT_BROKER               "192.168.1.50"
#define MQTT_PORT                 1883
#define MQTT_CLIENT_ID            "mushroom-unit-1"
#define DEVICE_ID                 "unit1"
#define MQTT_RETRY_INTERVAL_MS    5000

#endif // CONFIG_H