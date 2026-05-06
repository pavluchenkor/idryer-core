#pragma once

/**
 * @file device_publisher.h
 * @brief Thin dual-publish helper: sends device data to both MQTT and local WS.
 *
 * Local WS is a mirror of the device API, not a selective channel.
 * All outgoing device data that goes to MQTT should also go to any connected
 * local WS client. DevicePublisher enforces this by wrapping each publish call.
 *
 * Product calls one publishX(...) — DevicePublisher forwards to both transports.
 *
 * WS type strings mirror MQTT topic suffixes:
 *   info, telemetry, status, config, config/delta, events, integrations/status
 *
 * Responsibilities:
 *   - Duplicate publish to MQTT + LocalAccess.
 *
 * Not responsible for:
 *   - Command handling.
 *   - Product business logic.
 *   - Runtime or cloud state.
 *   - Connectivity checks (each transport handles its own).
 *
 * Usage:
 *   idryer::DevicePublisher pub(&s_mqtt, &s_local);
 *   pub.publishTelemetry(doc);   // → MQTT + WS if client connected
 *   pub.publishConfig(doc);      // → MQTT + WS if client connected
 */

#include <ArduinoJson.h>
#include <stdint.h>
#include "../mqtt/mqtt_client.h"

#if defined(ESP32) || defined(ESP_PLATFORM)
#include "local_access.h"
#endif

namespace idryer {

class DevicePublisher {
public:
#if defined(ESP32) || defined(ESP_PLATFORM)
    DevicePublisher(MqttClient* mqtt, LocalAccess* local)
        : mqtt_(mqtt), local_(local) {}
#else
    explicit DevicePublisher(MqttClient* mqtt)
        : mqtt_(mqtt), local_(nullptr) {}
#endif

    // ── Publish methods — mirror of MqttClient public API ────────────────────

    /** info topic (retained). Raw pre-serialized JSON string. */
    bool publishInfo(const char* json);

    /** telemetry topic. */
    bool publishTelemetry(JsonDocument& doc);

    /** status topic (retained). */
    bool publishStatus(JsonDocument& doc);

    /** config topic. */
    bool publishConfig(JsonDocument& doc);

    /** config topic — pre-serialized (e.g. large payload from RP2040). */
    uint16_t publishConfigRaw(const char* json, size_t len);

    /** config/delta topic — partial config update. */
    bool publishConfigDelta(const char* json, size_t len);

    /** events topic. */
    bool publishEvent(JsonDocument& doc);

    /** integrations/status topic (retained). */
    bool publishIntegrationsStatus(JsonDocument& doc);

    bool isMqttConnected() const  { return mqtt_->isConnected(); }
    bool isLocalConnected() const { return local_ && local_->isClientConnected(); }

private:
    MqttClient*  mqtt_;
#if defined(ESP32) || defined(ESP_PLATFORM)
    LocalAccess* local_;
#else
    void*        local_;
#endif

    void wsPublish(const char* type, JsonDocument& doc);
    void wsPublishRaw(const char* type, const char* json, size_t len);
};

} // namespace idryer
