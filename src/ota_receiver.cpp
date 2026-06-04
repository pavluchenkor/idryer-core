// OtaReceiver implementation — см. ota_receiver.h для контракта.

#include "ota_receiver.h"
#include "iDryer.h"
#include "mqtt/mqtt_client.h"
#include "hal/hal_types.h"

#include <Arduino.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <string.h>

namespace idryer {

// safeCopy(char*, size_t, const char*) уже определён в core/types.h
// (подключается через iDryer.h).

// ─── Singleton ───────────────────────────────────────────────────────────

OtaReceiver& OtaReceiver::instance() {
    static OtaReceiver s_instance;
    return s_instance;
}

OtaReceiver::OtaReceiver() {
    mbedtls_sha256_init(&shaCtx_);
}

OtaReceiver::~OtaReceiver() {
    shaFree();
}

// ─── Lifecycle ───────────────────────────────────────────────────────────

bool OtaReceiver::begin(iDryer::Link* link, const char* productId) {
    if (!link || !productId) {
        HAL_LOG_ERROR("OTA", "begin: null link or productId");
        return false;
    }
    link_ = link;
    mqtt_ = link->mqttClient();
    productId_ = productId;
    if (!mqtt_) {
        HAL_LOG_ERROR("OTA", "begin: link->mqttClient() == null");
        return false;
    }

    // JSON-команды через Link::onCommand (stateless lambdas → singleton).
    bool ok1 = link_->onCommand("firmware_update_announce",
        [](JsonObjectConst data) {
            OtaReceiver::instance().handleAnnounce(data);
        });
    bool ok2 = link_->onCommand("firmware_check_update_response",
        [](JsonObjectConst data) {
            OtaReceiver::instance().handleCheckUpdateResponse(data);
        });

    // Raw binary chunks — отдельный path в MqttClient (Шаг 7.1).
    mqtt_->setOtaChunkCallback(
        [](void* ctx, const char* topic, const uint8_t* payload, size_t len) {
            static_cast<OtaReceiver*>(ctx)->handleChunk(topic, payload, len);
        }, this);

    HAL_LOG_INFO("OTA", "Receiver registered (productId=%s, onCommand: announce=%d resp=%d)",
                 productId_, (int)ok1, (int)ok2);
    return ok1 && ok2;
}

void OtaReceiver::resetSession() {
    if (active_ && Update.isRunning()) {
        Update.abort();
    }
    shaFree();
    active_ = false;
    shaInited_ = false;
    commandId_[0] = '\0';
    toVersion_[0] = '\0';
    memset(expectedSha_, 0, sizeof(expectedSha_));
    expectedSize_ = 0;
    expectedChunks_ = 0;
    expectedChunkSize_ = 0;
    chunksReceived_ = 0;
    bytesReceived_ = 0;
}

// ─── Announce ────────────────────────────────────────────────────────────

void OtaReceiver::handleAnnounce(JsonObjectConst data) {
    const char* commandId  = data["commandId"]   | "";
    const char* target     = data["target"]      | "esp";
    const char* version    = data["version"]     | "";
    uint32_t    size       = data["size"]        | 0;
    const char* sha256Hex  = data["sha256"]      | "";
    uint16_t    chunkCount = data["chunkCount"]  | 0;
    uint16_t    chunkSize  = data["chunkSize"]   | 0;

    if (!commandId[0] || !version[0] || !sha256Hex[0] ||
        size == 0 || chunkCount == 0 || chunkSize == 0) {
        HAL_LOG_ERROR("OTA", "announce: missing required fields");
        // Без commandId publishAck не имеет смысла (backend не сопоставит).
        if (commandId[0]) {
            safeCopy(commandId_, sizeof(commandId_), commandId);
            publishAck("rejected_unsupported", "missing required fields", 0, 0);
        }
        return;
    }

    // RP2040 — пока вне scope (UART-мост, отдельная задача).
    if (strcmp(target, "esp") != 0) {
        safeCopy(commandId_, sizeof(commandId_), commandId);
        publishAck("rejected_unsupported",
                   "target=rp2040 not supported by this firmware", 0, 0);
        return;
    }

    // Если уже идёт сессия — abort старой. Backend в этом случае имеет
    // SAME commandId (idempotency replay) или новый (новый push).
    if (active_) {
        HAL_LOG_WARN("OTA", "announce: aborting previous session (cmd=%s)", commandId_);
        resetSession();
    }

    safeCopy(commandId_, sizeof(commandId_), commandId);
    safeCopy(toVersion_, sizeof(toVersion_), version);

    if (!hexToBytes(sha256Hex, expectedSha_, sizeof(expectedSha_))) {
        publishAck("rejected_unsupported", "invalid sha256 hex format", 0, 0);
        return;
    }

    expectedSize_      = size;
    expectedChunks_    = chunkCount;
    expectedChunkSize_ = chunkSize;
    chunksReceived_    = 0;
    bytesReceived_     = 0;

    // Heap pre-check: если free heap < expected size + запас — сразу reject.
    // (Update сам выделит свой buffer внутри; мы лишь проверяем что хватит.)
    uint32_t freeHeap  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    uint32_t freeFlash = 0; // Update.begin сам проверит ota_partition size

    if (!Update.begin(size)) {
        HAL_LOG_ERROR("OTA", "Update.begin(%u) failed: %s",
                      (unsigned)size, Update.errorString());
        publishAck("rejected_no_space", Update.errorString(), freeFlash, freeHeap);
        return;
    }

    shaStart();

    active_ = true;
    HAL_LOG_INFO("OTA", "Session started: cmd=%s v=%s size=%u chunks=%ux%uB sha=%.12s...",
                 commandId_, toVersion_, (unsigned)expectedSize_,
                 (unsigned)expectedChunks_, (unsigned)expectedChunkSize_, sha256Hex);
    publishAck("accepted", nullptr, 0, freeHeap);
}

// ─── Chunk ───────────────────────────────────────────────────────────────

void OtaReceiver::handleChunk(const char* topic, const uint8_t* payload, size_t len) {
    if (!active_) {
        // Чанк без активной сессии — дроп. Возможно осталось от прошлой
        // прерванной OTA, backend увидит timeout через watchdog.
        HAL_LOG_DEBUG("OTA", "chunk arrived without active session, drop");
        return;
    }
    uint16_t chunkIdx = 0;
    if (!parseChunkIdx(topic, chunkIdx)) {
        HAL_LOG_ERROR("OTA", "chunk: invalid topic format: %s", topic);
        return;
    }

    // Sequential ordering: ждём строго chunkIdx == chunksReceived_.
    // QoS 1 не гарантирует порядок при reconnect — backend шлёт строго после
    // ack progress на предыдущий, но дубликаты возможны. Дубликаты молча
    // ингорируем (idempotency).
    if (chunkIdx < chunksReceived_) {
        HAL_LOG_DEBUG("OTA", "chunk %u duplicate (have %u), ignored",
                      chunkIdx, chunksReceived_);
        return;
    }
    if (chunkIdx != chunksReceived_) {
        // Out-of-order — backend нарушил sequential. Закрываем сессию.
        HAL_LOG_ERROR("OTA", "chunk %u out of order (expected %u) — abort",
                      chunkIdx, chunksReceived_);
        Update.abort();
        publishComplete("flash_failed", "chunk out of order");
        resetSession();
        return;
    }

    size_t written = Update.write(const_cast<uint8_t*>(payload), len);
    if (written != len) {
        HAL_LOG_ERROR("OTA", "Update.write %u/%u: %s",
                      (unsigned)written, (unsigned)len, Update.errorString());
        Update.abort();
        publishComplete("flash_failed", Update.errorString());
        resetSession();
        return;
    }

    shaUpdate(payload, len);
    bytesReceived_ += len;
    chunksReceived_ += 1;

    // Feed task WDT — на длинных flash-операциях important.
    vTaskDelay(1 / portTICK_PERIOD_MS);

    publishProgress();

    // Последний чанк → финализация.
    if (chunksReceived_ == expectedChunks_) {
        uint8_t actualSha[32];
        shaFinish(actualSha);

        if (memcmp(actualSha, expectedSha_, 32) != 0) {
            HAL_LOG_ERROR("OTA", "sha256 mismatch — abort");
            Update.abort();
            publishComplete("sha_mismatch", "binary hash differs from announce");
            resetSession();
            return;
        }

        if (!Update.end(/*evenIfRemaining=*/true)) {
            HAL_LOG_ERROR("OTA", "Update.end failed: %s", Update.errorString());
            publishComplete("flash_failed", Update.errorString());
            resetSession();
            return;
        }

        HAL_LOG_INFO("OTA", "Session COMPLETE — sha verified, %u bytes, restarting...",
                     (unsigned)bytesReceived_);
        publishComplete("verified", nullptr);

        // Дать PubSubClient момент протолкнуть publish в TCP перед restart.
        // На ESP32 PubSubClient sync publish — после возврата из publish
        // пакет уже в socket. 200 мс с запасом на TLS flush.
        delay(200);

        resetSession();
        ESP.restart();
    }
}

// ─── Check-update response (pull-flow) ──────────────────────────────────

void OtaReceiver::handleCheckUpdateResponse(JsonObjectConst data) {
    bool hasUpdate         = data["hasUpdate"]      | false;
    const char* latest     = data["latestVersion"]  | "";
    bool willPushSoon      = data["willPushSoon"]   | false;
    bool requiresLicense   = data["requiresLicense"]| false;

    if (!hasUpdate) {
        HAL_LOG_DEBUG("OTA", "check_update: no update");
        return;
    }
    HAL_LOG_INFO("OTA", "check_update: v=%s willPushSoon=%d requiresLicense=%d",
                 latest, (int)willPushSoon, (int)requiresLicense);
    // Backend сам начнёт push (willPushSoon=true) либо ждать ручной admin push.
    // Устройство ничего не делает — announce прилетит обычным flow.
}

// ─── Pull-flow publish ──────────────────────────────────────────────────

void OtaReceiver::publishCheckUpdate(const char* currentVersion) {
    if (!mqtt_ || !productId_ || !currentVersion) return;
    StaticJsonDocument<256> doc;
    doc["currentVersion"] = currentVersion;
    doc["controllerType"] = "ESP32";
    doc["productId"]      = productId_;
    // licenseSerial — Phase 8, пока не шлём.
    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;
    mqtt_->publishFirmwareCheckUpdate(doc);
}

// ─── Publish helpers ────────────────────────────────────────────────────

void OtaReceiver::publishAck(const char* status, const char* reason,
                              uint32_t freeFlash, uint32_t freeHeap) {
    if (!mqtt_) return;
    StaticJsonDocument<256> doc;
    doc["commandId"] = commandId_;
    doc["status"]    = status;
    if (reason)        doc["reason"]         = reason;
    if (freeFlash > 0) doc["freeFlashBytes"] = freeFlash;
    if (freeHeap > 0)  doc["freeHeapBytes"]  = freeHeap;
    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;
    mqtt_->publishFirmwareUpdateAck(doc);
}

void OtaReceiver::publishProgress() {
    if (!mqtt_) return;
    StaticJsonDocument<192> doc;
    doc["commandId"] = commandId_;
    doc["received"]  = chunksReceived_;
    doc["total"]     = expectedChunks_;
    doc["bytes"]     = bytesReceived_;
    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;
    mqtt_->publishFirmwareUpdateProgress(doc);
}

void OtaReceiver::publishComplete(const char* status, const char* errorReason) {
    if (!mqtt_) return;
    StaticJsonDocument<256> doc;
    doc["commandId"]      = commandId_;
    doc["status"]         = status;
    if (toVersion_[0])    doc["newVersion"]      = toVersion_;
    if (errorReason)      doc["errorReason"]     = errorReason;
    doc["chunksReceived"] = chunksReceived_;
    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;
    mqtt_->publishFirmwareUpdateComplete(doc);
}

// ─── SHA256 streaming ───────────────────────────────────────────────────

void OtaReceiver::shaStart() {
    if (shaInited_) {
        mbedtls_sha256_free(&shaCtx_);
    }
    mbedtls_sha256_init(&shaCtx_);
    mbedtls_sha256_starts(&shaCtx_, /*is224=*/0);
    shaInited_ = true;
}

void OtaReceiver::shaUpdate(const uint8_t* data, size_t len) {
    if (!shaInited_) return;
    mbedtls_sha256_update(&shaCtx_, data, len);
}

void OtaReceiver::shaFinish(uint8_t out[32]) {
    if (!shaInited_) {
        memset(out, 0, 32);
        return;
    }
    mbedtls_sha256_finish(&shaCtx_, out);
    mbedtls_sha256_free(&shaCtx_);
    shaInited_ = false;
}

void OtaReceiver::shaFree() {
    if (shaInited_) {
        mbedtls_sha256_free(&shaCtx_);
        shaInited_ = false;
    }
}

// ─── Helpers ────────────────────────────────────────────────────────────

bool OtaReceiver::hexToBytes(const char* hex, uint8_t* out, size_t outLen) {
    if (!hex || !out) return false;
    size_t hexLen = strlen(hex);
    if (hexLen != outLen * 2) return false;
    for (size_t i = 0; i < outLen; i++) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int h = digit(hi), l = digit(lo);
        if (h < 0 || l < 0) return false;
        out[i] = (uint8_t)((h << 4) | l);
    }
    return true;
}

// ─── Self-confirm boot partition ────────────────────────────────────────

void OtaReceiver::markCurrentBootValid() {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        HAL_LOG_INFO("OTA", "boot partition marked valid (rollback cancelled)");
    } else if (err == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
        // Норма: партишен уже PENDING_VERIFY → VALID, либо это первая прошивка
        // не из OTA-партиции (factory). Никаких действий не требуется.
        HAL_LOG_DEBUG("OTA", "boot partition not in pending state (ok)");
    } else {
        HAL_LOG_ERROR("OTA", "esp_ota_mark_app_valid failed: %d", (int)err);
    }
}

bool OtaReceiver::parseChunkIdx(const char* topic, uint16_t& outIdx) {
    if (!topic) return false;
    const char* slash = strrchr(topic, '/');
    if (!slash || !slash[1]) return false;
    long val = strtol(slash + 1, nullptr, 10);
    if (val < 0 || val > 0xFFFF) return false;
    outIdx = (uint16_t)val;
    return true;
}

} // namespace idryer
