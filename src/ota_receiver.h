// OtaReceiver — приёмник OTA-обновлений по контракту mqtt_contract.yaml
// (см. ___OTA_MQTT_DESIGN.md).
//
// Регистрируется ОДИН на устройство как синглтон (в Link::onCommand
// принимаются stateless lambdas без capture — состояние нужно держать в
// глобальном объекте). Обрабатывает MQTT топики:
//
//   commands/firmware_update_announce               (JSON, через Link::onCommand)
//   commands/firmware_update_chunk/{cmdId}/{idx}    (raw binary, через
//                                                    MqttClient::setOtaChunkCallback)
//   commands/firmware_check_update_response         (JSON, через Link::onCommand)
//
// Публикует через MqttClient::publishFirmwareUpdate{Ack,Progress,Complete}
// и publishFirmwareCheckUpdate.
//
// Жизненный цикл сессии:
//   1) announce → проверки → Update.begin() + mbedtls_sha256_starts()
//      → publish ack(accepted) ИЛИ ack(rejected_*).
//   2) chunk N → проверка sequential ordering → Update.write + sha_update
//      → publish progress.
//   3) После последнего chunk → sha verify → Update.end(true) → publish
//      complete(verified) → ESP.restart() через малую задержку.
//   4) При любой ошибке (sha mismatch, write fail) → Update.abort() →
//      publish complete(sha_mismatch / flash_failed).
//
// Target=rp2040 пока не поддерживается — отдельная задача (UART-мост в
// RP2040, см. __OTA_MQTT_DESIGN.md раздел «OTA для RP2040»). На такой
// announce отвечаем rejected_unsupported.

#pragma once

// ESP-only: использует Arduino Update lib + mbedtls на ESP32. На RP2040 этот
// класс не нужен — RP получает прошивку через UART proxy (DRYER paired OTA),
// см. ___OTA_MQTT_DESIGN.md.
#if defined(ESP32) || defined(ESP_PLATFORM)

#include <ArduinoJson.h>
#include <stdint.h>
#include <stddef.h>
#include <mbedtls/sha256.h>

namespace iDryer { class Link; }
namespace idryer { class MqttClient; class UartBridge; }

namespace idryer {

class OtaReceiver {
public:
    /// Singleton — единственный экземпляр на прошивку. Stateless lambdas
    /// в Link::onCommand вызывают instance().handle*().
    static OtaReceiver& instance();

    /// Регистрирует callbacks в Link/MqttClient. Вызывать ПОСЛЕ link.begin()
    /// (MQTT должен быть инициализирован — onCommand требует commandRegistry).
    /// @param link        указатель на Link для onCommand + mqttClient().
    /// @param productId   строка из контракта (например "iheater_link") —
    ///                    используется в publishCheckUpdate.
    /// @param uartBridge  опционально: для DRYER (idryer-link) — указатель на
    ///                    уже инициализированный UartBridge, через который
    ///                    проксируются chunks при target=rp2040. Если nullptr —
    ///                    target=rp2040 отклоняется как unsupported.
    /// @param selfMajor   major текущей прошивки этого чипа (VERSION_MAJOR).
    ///                    Используется для solo/paired-решения при target=esp на
    ///                    DRYER: если target-major == selfMajor (minor/patch) —
    ///                    ESP перезагружается соло; иначе (major-bump) ждёт
    ///                    OtaCommitNow от RP. 0 = не задан → всегда solo.
    /// @return true если все callbacks зарегистрировались.
    bool begin(iDryer::Link* link, const char* productId,
               UartBridge* uartBridge = nullptr, uint8_t selfMajor = 0);

    /// Paired OTA Этап 4: ESP публикует firmware_check_update от имени RP
    /// (controllerType=RP2040). Вызывается из обработчика OtaCheckRequest
    /// при self-healing на RP. @param mcuVersion — major:minor:patch
    /// упакованные 16:8:8 (как UartHelloPayload.firmwareVersion).
    void publishCheckUpdateForMcu(uint32_t mcuVersion);

    /// Self-confirm boot partition: после успешного MQTT-connect устройство
    /// доказывает что новая прошивка работает (без этого bootloader откатит
    /// при следующем ребуте). Stateless — повторные вызовы no-op после
    /// первого успеха (ESP_ERR_OTA_ROLLBACK_INVALID_STATE — норма).
    /// Продукт зовёт **после** link.begin().
    static void markCurrentBootValid();

    /// Сбросить состояние активной сессии. Вызывать только в исключительных
    /// случаях (тестирование) — обычно сессия закрывается естественно через
    /// complete/abort.
    void resetSession();

    /// Paired OTA Этап 5: продукт зовёт раз в loop, чтобы периодически
    /// (раз в 2 секунды) посылать OtaStatus в RP. Без этого RP не узнает что
    /// ESP-сторона готова к синхронному commit. Также позволяет RP видеть
    /// текущий espTargetMajor (или 0 если ESP не в OTA-flow).
    void tick(uint32_t nowMs);

    // ─── Handlers (public — для статических трамплинов из onCommand) ────
    void handleAnnounce(JsonObjectConst data);
    void handleChunk(const char* topic, const uint8_t* payload, size_t len);
    void handleCheckUpdateResponse(JsonObjectConst data);

private:
    OtaReceiver();
    ~OtaReceiver();
    OtaReceiver(const OtaReceiver&) = delete;
    OtaReceiver& operator=(const OtaReceiver&) = delete;

    // Publish helpers.
    void publishAck(const char* status, const char* reason,
                    uint32_t freeFlash, uint32_t freeHeap);
    void publishProgress();
    void publishComplete(const char* status, const char* errorReason);

    // SHA256 streaming wrappers (для testability и читаемости).
    void shaStart();
    void shaUpdate(const uint8_t* data, size_t len);
    void shaFinish(uint8_t out[32]);
    void shaFree();

    // Helper: hex64 → 32 bytes.
    static bool hexToBytes(const char* hex, uint8_t* out, size_t outLen);

    // Helper: парсит chunkIdx из конца топика (last '/').
    static bool parseChunkIdx(const char* topic, uint16_t& outIdx);

    iDryer::Link* link_ = nullptr;
    MqttClient* mqtt_ = nullptr;
    const char* productId_ = nullptr;
    UartBridge* uart_ = nullptr;  // ESP-сторона DRYER: проксирование target=rp2040
    uint8_t selfMajor_ = 0;       // VERSION_MAJOR текущей прошивки (solo vs paired)

    // Active session state. Не trivially destructible (mbedtls_sha256_context
    // нужно free'ить). resetSession() обнуляет и зовёт mbedtls_sha256_free.
    bool active_ = false;
    bool shaInited_ = false;
    bool targetRp_ = false;       // текущая сессия для RP (UART-proxy) vs ESP (Update)
    uint32_t commandIdHash_ = 0;  // FNV1a от commandId — для корреляции с UartOtaChunkAck
    char commandId_[40] = {};     // UUID 36 + запас
    char toVersion_[32] = {};
    uint8_t expectedSha_[32] = {};
    uint32_t expectedSize_ = 0;
    uint16_t expectedChunks_ = 0;
    uint16_t expectedChunkSize_ = 0;
    uint16_t chunksReceived_ = 0;
    uint32_t bytesReceived_ = 0;

    // Sync polling state для UART-proxy chunk ack (target=rp2040).
    bool ackReceived_ = false;
    uint16_t ackChunkIdx_ = 0;
    uint8_t ackStatus_ = 0;

    // Paired OTA Этап 5 — ESP-сторона ожидает синхронного OtaCommitNow от RP.
    // При target=esp на DRYER (uart_ != nullptr): после verify Update.end()
    // вызывается сразу, но ESP.restart() НЕ вызывается; вместо этого ставим
    // espVerifiedPending_=true. RP видит это через OtaStatus и шлёт
    // OtaCommitNow когда оба чипа в idle.
    bool espVerifiedPending_ = false;
    uint8_t espTargetMajor_ = 0;
    uint32_t lastStatusSentAt_ = 0;
    static constexpr uint32_t STATUS_INTERVAL_MS = 2000;

    // OtaCommitNow handler.
    void handleOtaCommitNow();

    // UART-proxy helpers (target=rp2040).
    void handleOtaChunkAck(uint16_t chunkIdx, uint8_t status, uint32_t commandIdHash);
    bool pushChunkToRp(uint16_t chunkIdx, const uint8_t* data, size_t len);
    static uint32_t fnv1a32(const char* s);

    mbedtls_sha256_context shaCtx_;
};

} // namespace idryer

#endif // ESP32 / ESP_PLATFORM
