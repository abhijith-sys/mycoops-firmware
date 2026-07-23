#ifndef TOPICS_H
#define TOPICS_H

// Base path — DEVICE_ID (from Config.h) gets inserted between the prefix
// and each suffix at runtime, e.g. "farm/mushroom/unit1/sensors"
#define MQTT_TOPIC_PREFIX           "farm/mushroom/"

#define MQTT_TOPIC_SENSORS_SUFFIX   "/sensors"
#define MQTT_TOPIC_STATUS_SUFFIX    "/status"

// Not published/subscribed yet — reserved so the hierarchy doesn't need
// to change later when commands/events are added.
#define MQTT_TOPIC_COMMANDS_SUFFIX  "/commands"
#define MQTT_TOPIC_EVENTS_SUFFIX    "/events"

#endif // TOPICS_H