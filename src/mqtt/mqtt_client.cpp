#if defined(ESP32) || defined(ESP_PLATFORM)

#include "mqtt_client.h"
#include "root_ca.h"
#include "../hal/hal_types.h"
#include <esp_system.h>
#include <time.h>
#include <string.h>

namespace idryer {

MqttClient* MqttClient::instance_ = nullptr;

void MqttClient::begin(const char* serialNumber, const char* token) {
    memset(serialNumber_, 0, sizeof(serialNumber_));
    memset(token_,        0, sizeof(token_));
    memset(clientId_,     0, sizeof(clientId_));

    if (serialNumber && serialNumber[0]) {
        strncpy(serialNumber_, serialNumber, sizeof(serialNumber_) - 1);
        strncpy(clientId_,     serialNumber, sizeof(clientId_)     - 1);
    }
    if (token && token[0]) {
        strncpy(token_, token, sizeof(token_) - 1);
    }

#if MQTT_USE_TLS
    wifiClient_.setCACert(ROOT_CA_LETSENCRYPT);
    wifiClient_.setTimeout(10);
#endif

    mqttClient_.setClient(wifiClient_);
    mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient_.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient_.setKeepAlive(IDRYER_MQTT_KEEPALIVE);
    mqttClient_.setCallback(MqttClient::mqttCallback);

    instance_    = this;
    initialized_ = true;

    HAL_LOG_INFO("MQTT", "Init: broker=%s:%d serial=%s", MQTT_BROKER, MQTT_PORT, serialNumber_);
}

void MqttClient::setCommandCallback(CommandCallback callback, void* ctx) {
    commandCallback_ = callback;
    commandCallbackCtx_ = ctx;
}

void MqttClient::disconnect() {
    if (mqttClient_.connected()) mqttClient_.disconnect();
    initialized_ = false;
}

bool MqttClient::connect() {
    if (!initialized_) return false;
    if (mqttClient_.connected()) return true;

    if (!clientId_[0] || !serialNumber_[0] || !token_[0]) {
        HAL_LOG_ERROR("MQTT", "Empty credentials");
        return false;
    }

    HAL_LOG_INFO("MQTT", "Connecting as %s...", clientId_);

    char lwtTopic[128];
    idryer_make_topic(lwtTopic, sizeof(lwtTopic), serialNumber_, IDRYER_TOPIC_OFFLINE);

    // =========================================================================
    // DO NOT CHANGE kMqttCleanSession — MUST stay false.
    // Persistent session (clean_session=0) is required for reliable command
    // delivery. With clean_session=1 the broker discards the subscription on
    // reconnect; if the SUBSCRIBE packet is lost the device stops receiving
    // commands silently. Verified on live hardware (2026-04-20).
    // =========================================================================
    static constexpr bool kMqttCleanSession = false;

    bool connected = mqttClient_.connect(
        clientId_, serialNumber_, token_,
        lwtTopic, 1, false, "{}",
        kMqttCleanSession
    );

    if (!connected) {
        HAL_LOG_ERROR("MQTT", "Connection failed: state=%d", mqttClient_.state());
        return false;
    }

    HAL_LOG_INFO("MQTT", "Connected!");

    const char* cmdTopic = makeTopic(IDRYER_TOPIC_CMD_WILDCARD);
    bool subscribeOk = false;
    for (uint8_t attempt = 0; attempt < 3 && !subscribeOk; ++attempt) {
        subscribeOk = mqttClient_.subscribe(cmdTopic, IDRYER_QOS_COMMANDS);
        if (!subscribeOk) {
            HAL_LOG_WARN("MQTT", "SUBSCRIBE failed (attempt %u): %s", attempt + 1, cmdTopic);
            delay(50);
        }
    }

    if (!subscribeOk) {
        HAL_LOG_ERROR("MQTT", "SUBSCRIBE could not be sent — disconnecting to force reconnect");
        mqttClient_.disconnect();
        return false;
    }

    HAL_LOG_INFO("MQTT", "Subscribed: %s OK", cmdTopic);
    return true;
}

bool MqttClient::isConnected() { return mqttClient_.connected(); }

void MqttClient::loop() {
    if (!initialized_) return;
    if (!mqttClient_.connected()) connect();
    mqttClient_.loop();
}

// ============================================================================
// Publish
// ============================================================================

bool MqttClient::publishInfoJson(const char* json) {
    if (!mqttClient_.connected() || !json) return false;
    const char* topic = makeTopic(IDRYER_TOPIC_INFO);
    HAL_LOG_INFO("MQTT", "→ info: %s", json);
    return mqttClient_.publish(topic, json, IDRYER_RETAINED_INFO);
}

bool MqttClient::publishTelemetry(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_TELEMETRY, json, IDRYER_RETAINED_TELEMETRY);
}

bool MqttClient::publishStatus(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_STATUS, json, IDRYER_RETAINED_STATUS);
}

bool MqttClient::publishConfig(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_CONFIG, json, IDRYER_RETAINED_CONFIG);
}

bool MqttClient::publishEvent(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_EVENTS, json, IDRYER_RETAINED_EVENTS);
}

bool MqttClient::publishIntegrationsStatus(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_INTEGRATIONS_STATUS, json, /*retained=*/true);
}

uint16_t MqttClient::publishConfigRaw(const char* json, size_t length) {
    if (!mqttClient_.connected() || !json || length == 0) return 0;

    const char* topic = makeTopic(IDRYER_TOPIC_CONFIG);

    if (length <= MQTT_CONFIG_CHUNK_SIZE) {
        HAL_LOG_INFO("MQTT", "→ config (full): %u bytes", length);
        return mqttClient_.publish(topic, json, IDRYER_RETAINED_CONFIG) ? 1 : 0;
    }

    uint16_t tid = ++configTransferId_;
    size_t offset = 0;
    uint16_t idx = 0;
    uint16_t sentCount = 0;

    HAL_LOG_INFO("MQTT", "→ config (chunked): %u bytes tid=%u", length, tid);

    char* chunkBuf = (char*)malloc(MQTT_CONFIG_CHUNK_SIZE + 100);
    if (!chunkBuf) { HAL_LOG_ERROR("MQTT", "OOM for chunk buffer"); return 0; }

    while (offset < length) {
        size_t chunkDataLen = (length - offset > MQTT_CONFIG_CHUNK_SIZE)
                              ? MQTT_CONFIG_CHUNK_SIZE : (length - offset);
        bool isLast = (offset + chunkDataLen >= length);

        DynamicJsonDocument chunkDoc(MQTT_CONFIG_CHUNK_SIZE + 200);
        chunkDoc["tid"]   = tid;
        chunkDoc["idx"]   = idx;
        chunkDoc["total"] = length;
        chunkDoc["last"]  = isLast;

        char* dataBuf = (char*)malloc(chunkDataLen + 1);
        if (!dataBuf) { free(chunkBuf); return 0; }
        memcpy(dataBuf, json + offset, chunkDataLen);
        dataBuf[chunkDataLen] = '\0';
        chunkDoc["d"] = dataBuf;

        size_t written = serializeJson(chunkDoc, chunkBuf, MQTT_CONFIG_CHUNK_SIZE + 100);
        free(dataBuf);

        bool ok = mqttClient_.publish(topic, chunkBuf, IDRYER_RETAINED_CONFIG);
        if (!ok) { HAL_LOG_ERROR("MQTT", "Failed to publish chunk %u", idx); free(chunkBuf); return 0; }

        offset += chunkDataLen;
        idx++;
        sentCount++;
        delay(10);
    }

    free(chunkBuf);
    HAL_LOG_INFO("MQTT", "→ config complete: %u chunks", sentCount);
    return sentCount;
}

bool MqttClient::publishConfigDelta(const char* json, size_t length) {
    if (!mqttClient_.connected() || !json || length == 0) return false;
    const char* topic = makeTopic(IDRYER_TOPIC_CONFIG_DELTA);
    return mqttClient_.publish(topic, json, IDRYER_RETAINED_CONFIG_DELTA);
}

// ============================================================================
// Message handling
// ============================================================================

void MqttClient::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (instance_) {
        char* buf = (char*)malloc(length + 1);
        if (buf) {
            memcpy(buf, payload, length);
            buf[length] = '\0';
            instance_->handleMessage(topic, buf, length);
            free(buf);
        }
    }
}

void MqttClient::handleMessage(const char* topic, const char* payload, size_t length) {
    HAL_LOG_INFO("MQTT", "← %s (%u bytes): %.*s",
                 topic, (unsigned)length, (int)length, payload);

    const char* cmdPrefix = "/commands/";
    const char* cmdStart  = strstr(topic, cmdPrefix);
    if (!cmdStart) return;
    cmdStart += strlen(cmdPrefix);

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        HAL_LOG_ERROR("MQTT", "JSON parse error: %s", err.c_str());
        return;
    }

    if (commandCallback_) commandCallback_(commandCallbackCtx_, cmdStart, doc.as<JsonObjectConst>());
}

// ============================================================================
// Helpers
// ============================================================================

const char* MqttClient::makeTopic(const char* suffix) {
    idryer_make_topic(topicBuffer_, sizeof(topicBuffer_), serialNumber_, suffix);
    return topicBuffer_;
}

bool MqttClient::publishJson(const char* suffix, JsonDocument& json, bool retained) {
    if (!mqttClient_.connected()) return false;

    if (!json.containsKey("timestamp")) {
        char ts[32];
        json["timestamp"] = getIsoTimestamp(ts);
    }

    size_t jsonSize = measureJson(json);
    char* buf = (char*)malloc(jsonSize + 1);
    if (!buf) { HAL_LOG_ERROR("MQTT", "OOM"); return false; }
    serializeJson(json, buf, jsonSize + 1);

    const char* topic = makeTopic(suffix);
    HAL_LOG_DEBUG("MQTT", "→ %s (%u bytes): %s", topic, (unsigned)jsonSize, buf);

    bool ok = mqttClient_.publish(topic, buf, retained);
    free(buf);
    return ok;
}

char* MqttClient::getIsoTimestamp(char* buffer) {
    time_t now = time(nullptr);
    struct tm ti;
    gmtime_r(&now, &ti);
    strftime(buffer, 32, "%Y-%m-%dT%H:%M:%SZ", &ti);
    return buffer;
}

char* MqttClient::generateUuid(char* buffer) {
    uint32_t r1 = esp_random(), r2 = esp_random(), r3 = esp_random(), r4 = esp_random();
    snprintf(buffer, 37, "%08x-%04x-4%03x-%04x-%012llx",
             r1, (r2>>16)&0xFFFF, r2&0x0FFF,
             ((r3>>16)&0x3FFF)|0x8000,
             ((uint64_t)(r3&0xFFFF)<<32)|r4);
    return buffer;
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
