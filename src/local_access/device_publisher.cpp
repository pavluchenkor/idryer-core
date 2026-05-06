#include "device_publisher.h"
#include <string.h>

namespace idryer {

// ── Private helpers ───────────────────────────────────────────────────────────

void DevicePublisher::wsPublish(const char* type, JsonDocument& doc) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (local_) local_->publish(type, doc);
#else
    (void)type; (void)doc;
#endif
}

void DevicePublisher::wsPublishRaw(const char* type, const char* json, size_t len) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    if (local_) local_->publish(type, json, len);
#else
    (void)type; (void)json; (void)len;
#endif
}

// ── Publish methods ───────────────────────────────────────────────────────────

bool DevicePublisher::publishInfo(const char* json) {
    wsPublishRaw("info", json, strlen(json));
    return mqtt_->publishInfoJson(json);
}

bool DevicePublisher::publishTelemetry(JsonDocument& doc) {
    wsPublish("telemetry", doc);
    return mqtt_->publishTelemetry(doc);
}

bool DevicePublisher::publishStatus(JsonDocument& doc) {
    wsPublish("status", doc);
    return mqtt_->publishStatus(doc);
}

bool DevicePublisher::publishConfig(JsonDocument& doc) {
    wsPublish("config", doc);
    return mqtt_->publishConfig(doc);
}

uint16_t DevicePublisher::publishConfigRaw(const char* json, size_t len) {
    wsPublishRaw("config", json, len);
    return mqtt_->publishConfigRaw(json, len);
}

bool DevicePublisher::publishConfigDelta(const char* json, size_t len) {
    wsPublishRaw("config/delta", json, len);
    return mqtt_->publishConfigDelta(json, len);
}

bool DevicePublisher::publishEvent(JsonDocument& doc) {
    wsPublish("events", doc);
    return mqtt_->publishEvent(doc);
}

bool DevicePublisher::publishIntegrationsStatus(JsonDocument& doc) {
    wsPublish("integrations/status", doc);
    return mqtt_->publishIntegrationsStatus(doc);
}

} // namespace idryer
