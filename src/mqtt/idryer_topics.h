#ifndef IDRYER_TOPICS_H
#define IDRYER_TOPICS_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDRYER_TOPIC_PREFIX "idryer"

// Device -> Backend
#define IDRYER_TOPIC_INFO               "info"
#define IDRYER_TOPIC_TELEMETRY          "telemetry"
#define IDRYER_TOPIC_STATUS             "status"
#define IDRYER_TOPIC_EVENTS             "events"
#define IDRYER_TOPIC_CONFIG             "config"
#define IDRYER_TOPIC_CONFIG_DELTA       "config/delta"
#define IDRYER_TOPIC_OFFLINE            "offline"

// Backend -> Device
#define IDRYER_TOPIC_CMD_SET            "commands/set"
#define IDRYER_TOPIC_CMD_INVOKE         "commands/invoke"
#define IDRYER_TOPIC_CMD_WILDCARD       "commands/#"

// Integrations
#define IDRYER_TOPIC_INTEGRATIONS_STATUS "integrations/status"

// QoS for subscribe — enforced by PubSubClient.
// NOTE: publish always uses QoS 0 (PubSubClient limitation; no publish QoS API).
#define IDRYER_QOS_COMMANDS             1

// Retained flags
#define IDRYER_RETAINED_INFO            1
#define IDRYER_RETAINED_TELEMETRY       0
#define IDRYER_RETAINED_STATUS          1
#define IDRYER_RETAINED_EVENTS          0
#define IDRYER_RETAINED_CONFIG          1
#define IDRYER_RETAINED_CONFIG_DELTA    0

// Publish intervals (ms)
#define IDRYER_INTERVAL_TELEMETRY_MS    5000

static inline char* idryer_make_topic(char* buf, size_t buf_size,
                                      const char* serial, const char* suffix) {
    int len = snprintf(buf, buf_size, "%s/%s/%s", IDRYER_TOPIC_PREFIX, serial, suffix);
    return (len > 0 && (size_t)len < buf_size) ? buf : NULL;
}

static inline char* idryer_make_cmd_subscribe_topic(char* buf, size_t buf_size,
                                                     const char* serial) {
    return idryer_make_topic(buf, buf_size, serial, IDRYER_TOPIC_CMD_WILDCARD);
}

static inline const char* idryer_extract_topic_suffix(const char* full_topic) {
    const char* p = full_topic;
    if (strncmp(p, IDRYER_TOPIC_PREFIX "/", sizeof(IDRYER_TOPIC_PREFIX)) != 0) return NULL;
    p += sizeof(IDRYER_TOPIC_PREFIX);
    p = strchr(p, '/');
    if (!p) return NULL;
    return p + 1;
}

#ifdef __cplusplus
}
#endif

#endif // IDRYER_TOPICS_H
