#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "idryer_topics.h"
#include "core/callback.h"

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <espMqttClient.h>
#else
// Не-ESP платформы (RP2040/ControllerV2) компилируют core целиком через LDF
// deep+, но MQTT-часть не используют (mqtt_client.cpp под guard'ом). Заглушки
// типов espMqttClient держат заголовок компилируемым вне ESP.
namespace espMqttClientTypes {
enum class UseInternalTask { NO, YES };
struct MessageProperties {};
} // namespace espMqttClientTypes
class espMqttClient {
public:
    explicit espMqttClient(espMqttClientTypes::UseInternalTask) {}
};
class espMqttClientSecure {
public:
    explicit espMqttClientSecure(espMqttClientTypes::UseInternalTask) {}
};
#endif

// 30с → брокер объявляет тихую смерть (LWT) через 1.5×30 = 45с.
// Нижняя граница AWS IoT (30-1200с); PINGREQ шлётся только в паузах трафика.
#define IDRYER_MQTT_KEEPALIVE   30
#define TOPIC_BUFFER_SIZE       128
// Размер куска при чанкованной публикации конфига. Под каждый кусок ядро
// выделяет рабочий буфер такого же размера ОДНИМ блоком, поэтому потолок задаёт
// не сеть, а самый большой непрерывный блок heap продукта: на платах с
// дисплеем (LVGL занимает RAM статически) он около 7.6 КБ, и дефолтные 16 КБ
// там не выделяются — меню просто не публикуется. Переопределяется флагом
// сборки в platformio.ini конкретного продукта.
#ifndef MQTT_CONFIG_CHUNK_SIZE
#define MQTT_CONFIG_CHUNK_SIZE  16000
#endif

namespace idryer {

/**
 * @brief MQTT client for iDryer devices.
 *
 * Wraps @c espMqttClient (loop-режим, без внутренней FreeRTOS-задачи — вся
 * работа в главном цикле, как раньше с PubSubClient) and manages connection
 * and message routing. All topic names are built from the device serial
 * number automatically.
 *
 * QoS публикаций: state-critical сообщения (status, events, info, rfid,
 * weights, OTA-события) идут QoS 1 — с подтверждением брокера и ретраем.
 * Телеметрия — QoS 0 (частая, потеря единичной точки не важна).
 *
 * @note @c connect() асинхронный: инициирует подключение и возвращается.
 *       TCP+TLS handshake выполняется внутри @c loop(). Реконнект-политика
 *       (backoff) живёт в CloudStateMachine, НЕ здесь — loop() сам не
 *       переподключается.
 *
 * @note The MQTT session is persistent (@c clean_session = false). This ensures
 *       commands aren't lost if the device reconnects briefly.
 */
class MqttClient {
public:
    /// @brief Callback invoked when a @c commands/* message arrives.
    using CommandCallback = Callback<void(const char*, JsonObjectConst)>;

    /// @brief Callback для бинарных OTA-chunks. Срабатывает на топиках вида
    /// @c commands/firmware_update_chunk/{commandId}/{chunkIdx}, payload идёт
    /// как сырой бинарь (см. ___OTA_MQTT_DESIGN.md, format: raw_binary).
    /// Raw pointer передаётся в callback напрямую (живёт только время вызова).
    /// Сигнатура: (topic, payload, length).
    using OtaChunkCallback = Callback<void(const char*, const uint8_t*, size_t)>;

    /**
     * @brief Initializes the MQTT client with device credentials.
     *
     * @param serialNumber Device serial number — used as the MQTT client ID and username.
     * @param token        Device token — used as the MQTT password.
     *
     * Does not connect immediately. @c connect() does the actual connection.
     */
    void begin(const char* serialNumber, const char* token);

    /**
     * @brief Registers the callback for incoming @c commands/* messages.
     *
     * Called by @c IdryerRuntime — you don't need to set this yourself.
     */
    void setCommandCallback(CommandCallback::FnPtr fn, void* ctx = nullptr);

    /**
     * @brief Регистрирует callback для бинарных OTA-chunks
     * (@c commands/firmware_update_chunk/{commandId}/{chunkIdx}).
     *
     * Вызывается OtaReceiver. Если не зарегистрирован — chunks молча
     * игнорируются (никакого OTA-flow в прошивке).
     */
    void setOtaChunkCallback(OtaChunkCallback::FnPtr fn, void* ctx = nullptr);

    /**
     * @brief Инициирует подключение к брокеру (асинхронно).
     *
     * Handshake идёт в @c loop(); готовность проверять через @c isConnected().
     * Подписка на @c commands/# выполняется автоматически в onConnect.
     * Повторный вызов во время подключения безвреден (no-op).
     *
     * @return @c true если попытка запущена, @c false если уже идёт/нет данных.
     */
    bool connect();

    /// @brief Disconnects from the broker and marks the client as uninitialized.
    void disconnect();

    /// @brief Returns @c true if currently connected to the broker.
    bool isConnected();

    /**
     * @brief Must be called every loop iteration — и при подключении, и после.
     *
     * Продвигает handshake (TCP/TLS/MQTT), принимает входящие, шлёт keepalive
     * и ретраит QoS 1-пакеты. Сам НЕ переподключается — это делает
     * CloudStateMachine с backoff'ом.
     */
    void loop();

    /// @brief Код последнего разрыва (espMqttClientTypes::DisconnectReason).
    /// 5 = MQTT_NOT_AUTHORIZED (в т.ч. ACL-бан), 7 = TCP_DISCONNECTED.
    uint8_t lastDisconnectReason() const { return lastDisconnectReason_; }

    /// binding-v2: сброс кода разрыва после обработки (иначе старый отказ
    /// авторизации продолжал бы накручивать streak после перевыпуска токена).
    void clearLastDisconnectReason() { lastDisconnectReason_ = 0; }

    /**
     * @brief Publishes a pre-serialized JSON string to @c idryer/{serial}/info (retained, QoS 1).
     * @return @c true on success.
     */
    bool publishInfoJson(const char* json);

    /// @brief Publishes to @c idryer/{serial}/card (retained, QoS 1) — entity manifest.
    bool publishCard(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/telemetry (QoS 0).
    bool publishTelemetry(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/status (retained, QoS 1).
    bool publishStatus(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/config (retained, QoS 0 — крупный payload, retained сам по себе страхует).
    bool publishConfig(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/events (QoS 1).
    bool publishEvent(JsonDocument& json);

    /// binding-v3: подтверждение отзыва — «команду REVOKE получил, стираю
    /// секрет». Публикуется ДО стирания: после него говорить уже нечем.
    /// Портал по нему закрывает карточку (REVOKING → REVOKED) и снимает
    /// retained REVOKE. Топик idryer/{key}/revoke_ack, QoS1, не retained.
    bool publishRevokeAck();

    /// @brief Publishes to @c idryer/{serial}/integrations/status (retained, QoS 1).
    bool publishIntegrationsStatus(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/rfid (retained, QoS 1).
    bool publishRfid(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/weights (non-retained, QoS 1).
    bool publishWeights(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/rfid/write_result (non-retained, QoS 1).
    /// Используется bridge для ответа порталу на commands/write_rfid (Variant B).
    /// Payload: {commandId, status: "ok"|"failed", error?}.
    bool publishRfidWriteResult(JsonDocument& json);

    // ─── Phase 6 OTA event publishers (все QoS 1) ──────────────────────
    /// @brief Publishes to @c idryer/{serial}/events/firmware_update_ack.
    /// Используется OtaReceiver в ответ на commands/firmware_update_announce.
    bool publishFirmwareUpdateAck(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/events/firmware_update_progress.
    /// OtaReceiver шлёт на каждый принятый chunk — gating-сигнал для backend.
    bool publishFirmwareUpdateProgress(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/events/firmware_update_complete.
    /// Финальный статус OTA до ребута (verified/sha_mismatch/flash_failed/...).
    bool publishFirmwareUpdateComplete(JsonDocument& json);

    /// @brief Publishes to @c idryer/{serial}/events/firmware_check_update.
    /// Pull-flow: устройство спрашивает backend о наличии новой версии (~24h).
    bool publishFirmwareCheckUpdate(JsonDocument& json);

    /**
     * @brief Publishes a raw JSON string to @c idryer/{serial}/config.
     *
     * Automatically splits into chunks if the payload exceeds
     * @c MQTT_CONFIG_CHUNK_SIZE. Used for large config payloads from the RP2040.
     *
     * @return Number of chunks sent, or @c 0 on failure.
     */
    uint16_t publishConfigRaw(const char* json, size_t length);

    /** Кусок конфига в формате {tid, idx, total, last, d}. В отличие от
     *  publishConfigRaw публикуется БЕЗ retained (см. реализацию), а при
     *  first=true заодно стирает устаревший retained-снимок на топике. */
    uint16_t publishConfigChunk(const char* json, size_t length, bool first);

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

    /**
     * @brief Включить/выключить авто-добавление поля `timestamp` в publish-сообщения.
     *
     * По умолчанию включено (обратная совместимость). Портал хранит собственное
     * серверное время приёма, поэтому device-timestamp избыточен — его можно
     * отключить ради экономии трафика. Затрагивает только авто-добавление в
     * publishJson; явно проставленные timestamp (OTA/интеграции) не трогает.
     */
    void setAddTimestamp(bool enabled) { addTimestamp_ = enabled; }

private:
#if MQTT_USE_TLS
    espMqttClientSecure mqttClient_{espMqttClientTypes::UseInternalTask::NO};
#else
    espMqttClient mqttClient_{espMqttClientTypes::UseInternalTask::NO};
#endif
    CommandCallback commandCallback_;
    OtaChunkCallback otaChunkCallback_;

    char serialNumber_[48]; // binding-v3: вмещает deviceId (UUID, 36) + запас
    char token_[512];
    char clientId_[48];     // binding-v3: UUID-логин
    char topicBuffer_[TOPIC_BUFFER_SIZE];
    // setWill/subscribe хранят указатель — топики должны жить всё время клиента.
    char lwtTopic_[TOPIC_BUFFER_SIZE];
    char cmdTopic_[TOPIC_BUFFER_SIZE];

    uint16_t configTransferId_ = 0;
    bool initialized_ = false;
    bool addTimestamp_ = true; // авто-добавлять timestamp в publish (см. setAddTimestamp)
    uint8_t lastDisconnectReason_ = 0;

    void onMqttConnect(bool sessionPresent);
    void onMqttMessage(const espMqttClientTypes::MessageProperties& props,
                       const char* topic, const uint8_t* payload,
                       size_t len, size_t index, size_t total);
    void handleMessage(const char* topic, const char* payload, size_t length);
    const char* makeTopic(const char* suffix);
    bool publishJson(const char* suffix, JsonDocument& json, uint8_t qos, bool retained);
};

} // namespace idryer
