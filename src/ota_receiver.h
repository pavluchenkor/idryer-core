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
namespace idryer { class MqttClient; }

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
    /// @return true если все callbacks зарегистрировались.
    bool begin(iDryer::Link* link, const char* productId);

    /// Pull-flow: устройство периодически (раз в N часов) шлёт backend
    /// текущую версию и спрашивает о новой. Продукт зовёт сам из своего
    /// таймера / schedule. См. dispatch backend events/firmware_check_update.
    /// @param currentVersion semver текущей прошивки.
    void publishCheckUpdate(const char* currentVersion);

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

    // Active session state. Не trivially destructible (mbedtls_sha256_context
    // нужно free'ить). resetSession() обнуляет и зовёт mbedtls_sha256_free.
    bool active_ = false;
    bool shaInited_ = false;
    char commandId_[40] = {};     // UUID 36 + запас
    char toVersion_[32] = {};
    uint8_t expectedSha_[32] = {};
    uint32_t expectedSize_ = 0;
    uint16_t expectedChunks_ = 0;
    uint16_t expectedChunkSize_ = 0;
    uint16_t chunksReceived_ = 0;
    uint32_t bytesReceived_ = 0;

    mbedtls_sha256_context shaCtx_;
};

} // namespace idryer

#endif // ESP32 / ESP_PLATFORM
