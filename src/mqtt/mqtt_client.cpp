#if defined(ESP32) || defined(ESP_PLATFORM)

#include "mqtt_client.h"
#include "root_ca.h"
#include "../hal/hal_types.h"
#include <esp_system.h>
#include <time.h>
#include <string.h>

namespace idryer {

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

    // Персистентные топики: setWill хранит указатель, поэтому members.
    idryer_make_topic(lwtTopic_, sizeof(lwtTopic_), serialNumber_, IDRYER_TOPIC_OFFLINE);
    idryer_make_topic(cmdTopic_, sizeof(cmdTopic_), serialNumber_, IDRYER_TOPIC_CMD_WILDCARD);

#if MQTT_USE_TLS
    mqttClient_.setCACert(ROOT_CA_LETSENCRYPT);
#endif

    mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient_.setClientId(clientId_);
    mqttClient_.setCredentials(serialNumber_, token_);
    mqttClient_.setKeepAlive(IDRYER_MQTT_KEEPALIVE);
    // =========================================================================
    // DO NOT CHANGE cleanSession — MUST stay false.
    // Persistent session (clean_session=0) is required for reliable command
    // delivery. With clean_session=1 the broker discards the subscription on
    // reconnect; if the SUBSCRIBE packet is lost the device stops receiving
    // commands silently. Verified on live hardware (2026-04-20).
    // =========================================================================
    mqttClient_.setCleanSession(false);
    mqttClient_.setWill(lwtTopic_, /*qos=*/1, /*retain=*/false, "{}");

    mqttClient_.onConnect([this](bool sessionPresent) {
        onMqttConnect(sessionPresent);
    });
    mqttClient_.onDisconnect([this](espMqttClientTypes::DisconnectReason reason) {
        lastDisconnectReason_ = static_cast<uint8_t>(reason);
        HAL_LOG_WARN("MQTT", "Disconnected: %s",
                     espMqttClientTypes::disconnectReasonToString(reason));
    });
    mqttClient_.onMessage([this](const espMqttClientTypes::MessageProperties& props,
                                 const char* topic, const uint8_t* payload,
                                 size_t len, size_t index, size_t total) {
        onMqttMessage(props, topic, payload, len, index, total);
    });

    initialized_ = true;

    HAL_LOG_INFO("MQTT", "Init: broker=%s:%d serial=%s", MQTT_BROKER, MQTT_PORT, serialNumber_);
}

void MqttClient::setCommandCallback(CommandCallback::FnPtr fn, void* ctx) {
    commandCallback_.set(fn, ctx);
}

void MqttClient::setOtaChunkCallback(OtaChunkCallback::FnPtr fn, void* ctx) {
    otaChunkCallback_.set(fn, ctx);
}

// ─── Phase 6 OTA event publishers ────────────────────────────────────────
bool MqttClient::publishFirmwareUpdateAck(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_FW_UPDATE_ACK, json, /*qos=*/1, /*retained=*/false);
}
bool MqttClient::publishFirmwareUpdateProgress(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_FW_UPDATE_PROGRESS, json, /*qos=*/1, /*retained=*/false);
}
bool MqttClient::publishFirmwareUpdateComplete(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_FW_UPDATE_COMPLETE, json, /*qos=*/1, /*retained=*/false);
}
bool MqttClient::publishFirmwareCheckUpdate(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_FW_CHECK_UPDATE, json, /*qos=*/1, /*retained=*/false);
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

    // Асинхронно: ставит CONNECT в очередь и возвращается. TCP+TLS handshake
    // выполняется в loop(). Если подключение уже идёт — no-op (false).
    if (mqttClient_.connect()) {
        HAL_LOG_INFO("MQTT", "Connecting as %s...", clientId_);
        return true;
    }
    return false;
}

bool MqttClient::isConnected() { return mqttClient_.connected(); }

void MqttClient::loop() {
    if (!initialized_) return;
    // Продвигает handshake/приём/keepalive/QoS1-ретраи. Реконнект НЕ здесь:
    // политика повторов (backoff) — в CloudStateMachine.
    mqttClient_.loop();
}

void MqttClient::onMqttConnect(bool sessionPresent) {
    HAL_LOG_INFO("MQTT", "Connected! (session present=%d)", (int)sessionPresent);

    // Persistent session: при sessionPresent=true подписка уже жива на брокере,
    // но повторный SUBSCRIBE безвреден и защищает от рассинхрона.
    uint16_t packetId = mqttClient_.subscribe(cmdTopic_, IDRYER_QOS_COMMANDS);
    if (packetId == 0) {
        HAL_LOG_ERROR("MQTT", "SUBSCRIBE could not be queued — disconnecting to force reconnect");
        mqttClient_.disconnect();
        return;
    }
    HAL_LOG_INFO("MQTT", "Subscribed: %s OK", cmdTopic_);
}

// ============================================================================
// Publish
// ============================================================================

bool MqttClient::publishInfoJson(const char* json) {
    if (!mqttClient_.connected() || !json) return false;
    const char* topic = makeTopic(IDRYER_TOPIC_INFO);
    HAL_LOG_INFO("MQTT", "→ info: %s", json);
    return mqttClient_.publish(topic, /*qos=*/1, IDRYER_RETAINED_INFO, json) != 0;
}

bool MqttClient::publishTelemetry(JsonDocument& json) {
    // QoS 0: телеметрия частая, потеря единичной точки не критична —
    // не копим её в outbox при плохой связи.
    return publishJson(IDRYER_TOPIC_TELEMETRY, json, /*qos=*/0, IDRYER_RETAINED_TELEMETRY);
}

bool MqttClient::publishStatus(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_STATUS, json, /*qos=*/1, IDRYER_RETAINED_STATUS);
}

bool MqttClient::publishConfig(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_CONFIG, json, /*qos=*/0, IDRYER_RETAINED_CONFIG);
}

bool MqttClient::publishEvent(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_EVENTS, json, /*qos=*/1, IDRYER_RETAINED_EVENTS);
}

bool MqttClient::publishIntegrationsStatus(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_INTEGRATIONS_STATUS, json, /*qos=*/1, /*retained=*/true);
}

bool MqttClient::publishRfid(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_RFID, json, /*qos=*/1, IDRYER_RETAINED_RFID);
}

bool MqttClient::publishWeights(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_WEIGHTS, json, /*qos=*/1, IDRYER_RETAINED_WEIGHTS);
}

bool MqttClient::publishRfidWriteResult(JsonDocument& json) {
    return publishJson(IDRYER_TOPIC_RFID_WRITE_RESULT, json, /*qos=*/1, IDRYER_RETAINED_RFID_WRITE_RESULT);
}

uint16_t MqttClient::publishConfigRaw(const char* json, size_t length) {
    if (!mqttClient_.connected() || !json || length == 0) return 0;

    const char* topic = makeTopic(IDRYER_TOPIC_CONFIG);

    // QoS 0: чанки до 16КБ — не копим их в outbox (heap). Retained-снэпшот
    // на брокере сам по себе страхует доставку последней версии.
    if (length <= MQTT_CONFIG_CHUNK_SIZE) {
        HAL_LOG_INFO("MQTT", "→ config (full): %u bytes", length);
        return mqttClient_.publish(topic, /*qos=*/0, IDRYER_RETAINED_CONFIG,
                                   reinterpret_cast<const uint8_t*>(json), length) != 0 ? 1 : 0;
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

        bool ok = mqttClient_.publish(topic, /*qos=*/0, IDRYER_RETAINED_CONFIG,
                                      reinterpret_cast<const uint8_t*>(chunkBuf), written) != 0;
        if (!ok) { HAL_LOG_ERROR("MQTT", "Failed to publish chunk %u", idx); free(chunkBuf); return 0; }

        // Прокачиваем отправку: QoS 0-пакет уходит из outbox в сеть в loop().
        mqttClient_.loop();

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
    return mqttClient_.publish(topic, /*qos=*/1, IDRYER_RETAINED_CONFIG_DELTA,
                               reinterpret_cast<const uint8_t*>(json), length) != 0;
}

// ============================================================================
// Message handling
// ============================================================================

void MqttClient::onMqttMessage(const espMqttClientTypes::MessageProperties& props,
                               const char* topic, const uint8_t* payload,
                               size_t len, size_t index, size_t total) {
    (void)props;

    // EMC_RX_BUFFER_SIZE=16384 гарантирует, что все наши payload'ы (команды
    // ≤1КБ, OTA-chunks ≤4КБ) приходят одним куском. Частичная доставка =
    // сообщение больше буфера — дропаем, как раньше дропал PubSubClient.
    if (index != 0 || len != total) {
        HAL_LOG_ERROR("MQTT", "← chunked payload on %s (%u/%u) — dropped, raise EMC_RX_BUFFER_SIZE",
                      topic, (unsigned)(index + len), (unsigned)total);
        return;
    }

    // OTA-chunks (commands/firmware_update_chunk/{commandId}/{chunkIdx}) — это
    // сырой бинарь до 4 КБ (см. ___OTA_MQTT_DESIGN.md, format: raw_binary).
    // НЕ копируем в s_payload_buf (он 1 КБ — chunks бы дропались) и НЕ парсим
    // как JSON. Передаём raw pointer OtaReceiver, он уже знает что с этим
    // делать (Update.write + mbedtls_sha256_update). Pointer живёт только
    // время этого вызова — OtaReceiver обязан скопировать или записать сразу.
    if (strstr(topic, "/commands/firmware_update_chunk/")) {
        if (otaChunkCallback_) {
            otaChunkCallback_(topic, payload, len);
        }
        return;
    }

    // profile-команда с 10 стадиями занимает ~540 байт.
    static char s_payload_buf[1024];
    if (len >= sizeof(s_payload_buf)) {
        HAL_LOG_ERROR("MQTT", "← payload too large: %u bytes, dropped", (unsigned)len);
        return;
    }
    memcpy(s_payload_buf, payload, len);
    s_payload_buf[len] = '\0';
    handleMessage(topic, s_payload_buf, len);
}

void MqttClient::handleMessage(const char* topic, const char* payload, size_t length) {
    HAL_LOG_INFO("MQTT", "← %s (%u bytes): %.*s",
                 topic, (unsigned)length, (int)length, payload);

    const char* cmdPrefix = "/commands/";
    const char* cmdStart  = strstr(topic, cmdPrefix);
    if (!cmdStart) return;
    cmdStart += strlen(cmdPrefix);

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        HAL_LOG_ERROR("MQTT", "JSON parse error: %s", err.c_str());
        return;
    }

    if (commandCallback_) commandCallback_(cmdStart, doc.as<JsonObjectConst>());
}

// ============================================================================
// Helpers
// ============================================================================

const char* MqttClient::makeTopic(const char* suffix) {
    idryer_make_topic(topicBuffer_, sizeof(topicBuffer_), serialNumber_, suffix);
    return topicBuffer_;
}

bool MqttClient::publishJson(const char* suffix, JsonDocument& json, uint8_t qos, bool retained) {
    if (!mqttClient_.connected()) return false;

    // Авто-добавление timestamp можно отключить (setAddTimestamp) — портал хранит
    // собственное серверное время приёма, device-timestamp избыточен (экономия трафика).
    if (addTimestamp_ && !json.containsKey("timestamp")) {
        char ts[32];
        json["timestamp"] = getIsoTimestamp(ts);
    }

    // Статический буфер — без heap allocation.
    // 1024 байт покрывает telemetry/status/events/integrations.
    // Для config используется publishConfigRaw (отдельный путь).
    static char s_buf[1024];
    size_t needed = measureJson(json);
    if (needed >= sizeof(s_buf)) {
        HAL_LOG_ERROR("MQTT", "→ %s: payload too large (%u bytes)", suffix, (unsigned)needed);
        return false;
    }
    size_t written = serializeJson(json, s_buf, sizeof(s_buf));

    const char* topic = makeTopic(suffix);
    HAL_LOG_DEBUG("MQTT", "→ %s (%u bytes, qos%u)", topic, (unsigned)needed, qos);
    // QoS 1: пакет уходит в outbox и ретраится до PUBACK (espMqttClient
    // копирует payload — s_buf можно переиспользовать сразу).
    return mqttClient_.publish(topic, qos, retained,
                               reinterpret_cast<const uint8_t*>(s_buf), written) != 0;
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
