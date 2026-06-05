// OtaReceiver implementation — см. ota_receiver.h для контракта.
//
// ESP-only — см. guard в ota_receiver.h.

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ota_receiver.h"
#include "iDryer.h"
#include "mqtt/mqtt_client.h"
#include "hal/hal_types.h"
#include "uart/uart_bridge.h"
#include "uart/uart_protocol.h"
#include "work_time_tracker.h"

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

bool OtaReceiver::begin(iDryer::Link* link, const char* productId,
                        UartBridge* uartBridge) {
    if (!link || !productId) {
        HAL_LOG_ERROR("OTA", "begin: null link or productId");
        return false;
    }
    link_ = link;
    mqtt_ = link->mqttClient();
    productId_ = productId;
    uart_ = uartBridge;
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

    // DRYER paired OTA: подписка на OtaChunkAck от RP для sync polling в handleChunk.
    if (uart_) {
        uart_->setOtaChunkAckHandler(
            [](const UartOtaChunkAckPayload& p, const UartFrameHeader&) {
                OtaReceiver::instance().handleOtaChunkAck(p.chunkIdx, p.status, p.commandId);
            });
        // Этап 4: RP→ESP self-healing запрос на публикацию check_update для RP.
        uart_->setOtaCheckRequestHandler(
            [](const UartOtaCheckRequestPayload& p, const UartFrameHeader&) {
                OtaReceiver::instance().publishCheckUpdateForMcu(p.currentVersion);
            });
    }

    HAL_LOG_INFO("OTA", "Receiver registered (productId=%s, uart=%s, onCommand: announce=%d resp=%d)",
                 productId_, uart_ ? "yes" : "no", (int)ok1, (int)ok2);
    return ok1 && ok2;
}

void OtaReceiver::resetSession() {
    if (active_ && !targetRp_ && Update.isRunning()) {
        Update.abort();
    }
    shaFree();
    active_ = false;
    shaInited_ = false;
    targetRp_ = false;
    commandIdHash_ = 0;
    commandId_[0] = '\0';
    toVersion_[0] = '\0';
    memset(expectedSha_, 0, sizeof(expectedSha_));
    expectedSize_ = 0;
    expectedChunks_ = 0;
    expectedChunkSize_ = 0;
    chunksReceived_ = 0;
    bytesReceived_ = 0;
    ackReceived_ = false;
    ackChunkIdx_ = 0;
    ackStatus_ = 0;
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

    // target=rp2040 поддерживается ТОЛЬКО на DRYER (когда передан UartBridge
    // через begin). Иначе — reject как unsupported (iHeater/Storage).
    bool wantRp = (strcmp(target, "rp2040") == 0);
    if (wantRp && !uart_) {
        safeCopy(commandId_, sizeof(commandId_), commandId);
        publishAck("rejected_unsupported",
                   "target=rp2040 not supported by this firmware", 0, 0);
        return;
    }
    if (!wantRp && strcmp(target, "esp") != 0) {
        safeCopy(commandId_, sizeof(commandId_), commandId);
        publishAck("rejected_unsupported", "unknown target", 0, 0);
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
    commandIdHash_ = fnv1a32(commandId_);

    if (!hexToBytes(sha256Hex, expectedSha_, sizeof(expectedSha_))) {
        publishAck("rejected_unsupported", "invalid sha256 hex format", 0, 0);
        return;
    }

    expectedSize_      = size;
    expectedChunks_    = chunkCount;
    expectedChunkSize_ = chunkSize;
    chunksReceived_    = 0;
    bytesReceived_     = 0;
    targetRp_          = wantRp;

    uint32_t freeHeap  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);

    if (targetRp_) {
        // target=rp2040: ESP ничего не пишет себе. Update.begin/shaStart НЕ
        // вызываются — SHA256 финально верифицирует RP. ESP лишь пробрасывает
        // chunks через UART и пересылает ack/progress/complete в MQTT.
        active_ = true;
        HAL_LOG_INFO("OTA", "Proxy session started (RP2040): cmd=%s v=%s size=%u chunks=%ux%uB",
                     commandId_, toVersion_, (unsigned)expectedSize_,
                     (unsigned)expectedChunks_, (unsigned)expectedChunkSize_);

        // Доставить RP параметры сессии: expectedSha + totalSize + commandIdHash.
        // Без этого RP не сможет финально проверить SHA после последнего chunk'а.
        UartOtaAnnounceForMcuPayload ann{};
        ann.commandId   = commandIdHash_;
        ann.totalChunks = expectedChunks_;
        ann.chunkSize   = expectedChunkSize_;
        ann.totalSize   = expectedSize_;
        memcpy(ann.expectedSha, expectedSha_, sizeof(ann.expectedSha));
        // targetMajor парсим из toVersion_ ("major.minor.patch"): только major.
        ann.targetMajor = (uint8_t)strtoul(toVersion_, nullptr, 10);
        if (uart_) {
            if (!uart_->sendOtaAnnounceForMcu(ann)) {
                HAL_LOG_ERROR("OTA", "Failed to send OtaAnnounceForMcu to RP");
                publishAck("rejected_unsupported", "uart announce send failed", 0, freeHeap);
                resetSession();
                return;
            }
        }

        publishAck("accepted", nullptr, 0, freeHeap);
        return;
    }

    // target=esp: классический self-flash flow.
    if (!Update.begin(size)) {
        HAL_LOG_ERROR("OTA", "Update.begin(%u) failed: %s",
                      (unsigned)size, Update.errorString());
        publishAck("rejected_no_space", Update.errorString(), 0, freeHeap);
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
        if (!targetRp_) Update.abort();
        publishComplete("flash_failed", "chunk out of order");
        resetSession();
        return;
    }

    // target=rp2040: проксируем chunk через UART, ждём OtaChunkAck от RP.
    if (targetRp_) {
        if (!pushChunkToRp(chunkIdx, payload, len)) {
            HAL_LOG_ERROR("OTA", "proxy chunk %u failed (uart/ack)", chunkIdx);
            // pushChunkToRp уже опубликовал complete(...) с конкретным reason.
            return;
        }
        bytesReceived_ += len;
        chunksReceived_ += 1;
        publishProgress();
        // Финальная верификация SHA — на RP. ESP лишь публикует verified когда
        // дошли до последнего chunk'а; реальный commit/reboot инициирует RP
        // через OtaCommitNow (Этап 5). Здесь сессия закрывается, ESP не ребутается.
        if (chunksReceived_ == expectedChunks_) {
            HAL_LOG_INFO("OTA", "Proxy session COMPLETE — %u bytes forwarded to RP",
                         (unsigned)bytesReceived_);
            publishComplete("verified", nullptr);
            resetSession();
        }
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

        // Force-persist накопительный workTimeCounter в NVS до ESP.restart() —
        // иначе теряем до 5 мин секунд работы (PERSIST_INTERVAL_MS). Сам
        // ESP.restart НЕ graceful, дальше шансов нет.
        WorkTimeTracker::instance().flush();

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
    // board — PlatformIO env (esp32c3-super-mini / xiao-esp32s3 / ...). Без
    // этого backend не сможет отличить прошивку для esp32c3 от прошивки для
    // xiao-esp32s3 (одна productId+controllerType+version, разные .bin).
    // PIO_ENV прокидывается через build_flags=-DPIO_ENV=\\"$PIOENV\\".
#ifdef PIO_ENV
    doc["board"] = PIO_ENV;
#endif
    // licenseSerial — Phase 8, пока не шлём.
    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;
    mqtt_->publishFirmwareCheckUpdate(doc);
}

void OtaReceiver::publishCheckUpdateForMcu(uint32_t mcuVersion) {
    if (!mqtt_ || !productId_) return;
    // major:minor:patch упакованы 16:8:8 (см. UartHelloPayload.firmwareVersion).
    uint8_t major = (mcuVersion >> 16) & 0xFF;
    uint8_t minor = (mcuVersion >> 8) & 0xFF;
    uint8_t patch = mcuVersion & 0xFF;
    char verStr[16];
    snprintf(verStr, sizeof(verStr), "%u.%u.%u", major, minor, patch);

    StaticJsonDocument<256> doc;
    doc["currentVersion"] = verStr;
    doc["controllerType"] = "RP2040";
    doc["productId"]      = productId_;
    // board для RP2040 ESP не знает (две сборки pico_0x44/0x45 не различимы из
    // UART Hello). Backend разрешит выбор по productId+controllerType+version;
    // при необходимости board будет добавлен отдельным запросом.
    char ts[32];
    MqttClient::getIsoTimestamp(ts);
    doc["timestamp"] = ts;
    mqtt_->publishFirmwareCheckUpdate(doc);
    HAL_LOG_INFO("OTA", "Published check_update for MCU v=%s (RP2040)", verStr);
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

// ─── UART proxy (target=rp2040) ─────────────────────────────────────────

void OtaReceiver::handleOtaChunkAck(uint16_t chunkIdx, uint8_t status, uint32_t commandIdHash) {
    // Дроп ack от чужой/устаревшей сессии: разные commandIdHash возможны
    // при перезапуске сессии или ошибке backend.
    if (!active_ || !targetRp_ || commandIdHash != commandIdHash_) {
        HAL_LOG_DEBUG("OTA", "drop OtaChunkAck: active=%d rp=%d hash=%08x/%08x",
                      (int)active_, (int)targetRp_,
                      (unsigned)commandIdHash, (unsigned)commandIdHash_);
        return;
    }
    ackChunkIdx_ = chunkIdx;
    ackStatus_   = status;
    ackReceived_ = true;
}

bool OtaReceiver::pushChunkToRp(uint16_t chunkIdx, const uint8_t* data, size_t len) {
    if (!uart_) {
        publishComplete("flash_failed", "uart bridge not available");
        resetSession();
        return false;
    }
    if (len > expectedChunkSize_) {
        publishComplete("flash_failed", "chunk size exceeds announce");
        resetSession();
        return false;
    }

    // Заголовок логического OtaChunkForMcu (12 байт) + data (до 4 КБ).
    UartOtaChunkForMcuPayload header{};
    header.commandId   = commandIdHash_;
    header.chunkIdx    = chunkIdx;
    header.totalChunks = expectedChunks_;
    header.dataLength  = (uint16_t)len;
    header._pad        = 0;

    constexpr size_t HDR = sizeof(UartOtaChunkForMcuPayload);
    static_assert(HDR == 12, "OtaChunkForMcu header expected 12 bytes");
    constexpr size_t MAX = UART_MAX_PAYLOAD;          // 200
    constexpr size_t FIRST_DATA = MAX - HDR;          // 188

    // Первый фрагмент: header + до 188 байт data.
    size_t firstDataLen = (len > FIRST_DATA) ? FIRST_DATA : len;
    bool isLast = (firstDataLen == len);

    uint8_t firstFrame[MAX];
    memcpy(firstFrame, &header, HDR);
    if (firstDataLen > 0) memcpy(firstFrame + HDR, data, firstDataLen);

    uint8_t flags = UART_FLAG_FRAGMENT;
    if (isLast) flags |= UART_FLAG_LAST_FRAGMENT;

    if (!uart_->sendOtaChunkForMcu(firstFrame, (uint8_t)(HDR + firstDataLen), flags)) {
        publishComplete("flash_failed", "uart send (first fragment)");
        resetSession();
        return false;
    }
    delay(2);

    // Последующие фрагменты: только продолжение data до 200 байт.
    size_t offset = firstDataLen;
    while (offset < len) {
        size_t remaining = len - offset;
        size_t take = (remaining > MAX) ? MAX : remaining;
        bool last = (offset + take >= len);
        flags = UART_FLAG_FRAGMENT;
        if (last) flags |= UART_FLAG_LAST_FRAGMENT;
        if (!uart_->sendOtaChunkForMcu(data + offset, (uint8_t)take, flags)) {
            publishComplete("flash_failed", "uart send (fragment)");
            resetSession();
            return false;
        }
        offset += take;
        delay(2);
    }

    // Ждём OtaChunkAck от RP — sync polling. Тайм-аут 2 секунды на chunk
    // (115200 baud, ~4 КБ chunk ≈ 350 мс + RP flash-page write ≈ 50 мс).
    ackReceived_ = false;
    constexpr uint32_t ACK_TIMEOUT_MS = 2000;
    uint32_t start = millis();
    while (!ackReceived_ && (millis() - start) < ACK_TIMEOUT_MS) {
        uart_->loop();
        delay(1);
    }
    if (!ackReceived_) {
        HAL_LOG_ERROR("OTA", "proxy chunk %u: UART ack timeout", chunkIdx);
        publishComplete("flash_failed", "uart ack timeout");
        resetSession();
        return false;
    }
    if (ackChunkIdx_ != chunkIdx) {
        HAL_LOG_ERROR("OTA", "proxy chunk %u: ack for wrong idx %u",
                      chunkIdx, ackChunkIdx_);
        publishComplete("flash_failed", "uart ack idx mismatch");
        resetSession();
        return false;
    }
    if (ackStatus_ != 0) {
        const char* reason = "rp ack: unknown error";
        switch (ackStatus_) {
            case 1: reason = "rp ack: sha_mismatch"; break;
            case 2: reason = "rp ack: flash_failed"; break;
            case 3: reason = "rp ack: out_of_order"; break;
        }
        HAL_LOG_ERROR("OTA", "proxy chunk %u: %s", chunkIdx, reason);
        publishComplete(ackStatus_ == 1 ? "sha_mismatch" : "flash_failed", reason);
        resetSession();
        return false;
    }
    return true;
}

uint32_t OtaReceiver::fnv1a32(const char* s) {
    uint32_t h = 2166136261u;
    if (!s) return h;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h;
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

#endif // ESP32 / ESP_PLATFORM
