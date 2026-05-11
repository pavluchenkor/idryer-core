#pragma once

#include <Arduino.h>
#if MQTT_USE_TLS
#include <WiFiClientSecure.h>
#else
#include <WiFi.h>
#endif
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "idryer_topics.h"

#define IDRYER_MQTT_KEEPALIVE   60
#define MQTT_BUFFER_SIZE        16384
#define TOPIC_BUFFER_SIZE       128
#define MQTT_CONFIG_CHUNK_SIZE  16000

namespace idryer {

/**
 * @brief MQTT client for iDryer devices.
 *
 * Wraps @c PubSubClient and manages connection, reconnection, and message routing.
 * All topic names are built from the device serial number automatically.
 *
 * Typical usage — the runtime handles everything after @c begin():
 * @code
 * mqtt.begin(serialNumber, deviceToken);
 * mqtt.setCommandCallback(...); // wired by IdryerRuntime
 * // IdryerRuntime calls connect() and loop() for you
 * @endcode
 *
 * @note @c PubSubClient always publishes at QoS 0, regardless of topic QoS constants.
 *       Subscribe uses QoS 1 (IDRYER_QOS_COMMANDS).
 *
 * @note The MQTT session is persistent (@c clean_session = false). This ensures
 *       commands aren't lost if the device reconnects briefly.
 */
class MqttClient {
public:
    /// @brief Callback invoked when a @c commands/* message arrives.
    /// fnptr + ctx — без std::function чтобы не аллоцировать heap (см. iDryer.h).
    using CommandCallback = void (*)(void* ctx, const char* command, JsonObjectConst data);

    /**
     * @brief Initializes the MQTT client with device credentials.
     *
     * @param serialNumber Device serial number — used as the MQTT client ID and username.
     * @param token        Device token — used as the MQTT password.
     *
     * Does not connect immediately. @c connect() (or @c loop()) does the actual connection.
     */
    void begin(const char* serialNumber, const char* token);

    /**
     * @brief Registers the callback for incoming @c commands/* messages.
     *
     * Called by @c IdryerRuntime — you don't need to set this yourself.
     */
    void setCommandCallback(CommandCallback callback, void* ctx);

    /**
     * @brief Connects to the broker and subscribes to @c commands/#.
     *
     * Retries the SUBSCRIBE up to 3 times and disconnects if all fail.
     * @return @c true if connected and subscribed successfully.
     */
    bool connect();

    /// @brief Disconnects from the broker and marks the client as uninitialized.
    void disconnect();

    /// @brief Returns @c true if currently connected to the broker.
    bool isConnected();

    /**
     * @brief Must be called every loop iteration.
     *
     * Reconnects if disconnected, then calls @c PubSubClient::loop() to
     * receive incoming messages.
     */
    void loop();

    /**
     * @brief Publishes a pre-serialized JSON string to @c idryer/{serial}/info (retained).
     * @return @c true on success.
     */
    bool publishInfoJson(const char* json);

    /// @brief Publishes to @c idryer/{serial}/telemetry.
    bool publishTelemetry(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/status (retained).
    bool publishStatus(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/config.
    bool publishConfig(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/events.
    bool publishEvent(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/integrations/status (retained).
    bool publishIntegrationsStatus(JsonDocument& json);

    /**
     * @brief Publishes a raw JSON string to @c idryer/{serial}/config.
     *
     * Automatically splits into chunks if the payload exceeds
     * @c MQTT_CONFIG_CHUNK_SIZE. Used for large config payloads from the RP2040.
     *
     * @return Number of chunks sent, or @c 0 on failure.
     */
    uint16_t publishConfigRaw(const char* json, size_t length);

    /**
     * @brief Publishes a JSON string to @c idryer/{serial}/config/delta.
     *
     * Used to push partial config updates without retransmitting the full config.
     */
    bool publishConfigDelta(const char* json, size_t length);

    /// @brief Formats the current UTC time as ISO 8601 into @p buffer (must be ≥ 32 bytes).
    static char* getIsoTimestamp(char* buffer);

    /// @brief Generates a random UUID v4 into @p buffer (must be ≥ 37 bytes).
    static char* generateUuid(char* buffer);

private:
#if MQTT_USE_TLS
    WiFiClientSecure wifiClient_;
#else
    WiFiClient wifiClient_;
#endif
    PubSubClient mqttClient_;
    CommandCallback commandCallback_    = nullptr;
    void*           commandCallbackCtx_ = nullptr;

    char serialNumber_[32];
    char token_[512];
    char clientId_[32];
    char topicBuffer_[TOPIC_BUFFER_SIZE];

    uint16_t configTransferId_ = 0;
    bool initialized_ = false;

    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(const char* topic, const char* payload, size_t length);
    const char* makeTopic(const char* suffix);
    // PubSubClient publishes at QoS 0 regardless of topic QoS constants.
    bool publishJson(const char* suffix, JsonDocument& json, bool retained = false);

    static MqttClient* instance_;
};

} // namespace idryer
