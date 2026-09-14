#include "device_publisher.h"
#include <string.h>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include "../hal/hal_types.h"
#include <esp_heap_caps.h>
#endif


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

bool DevicePublisher::publishCard(JsonDocument& doc) {
    wsPublish("card", doc);
    return mqtt_->publishCard(doc);
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

uint16_t DevicePublisher::publishConfigChunk(const char* json, size_t len, bool first) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    // Порог по памяти. Копии кусков лежат в куче, пока их не заберёт сеть. На
    // слабом сигнале клиент не успевает читать, куча уходит в ноль, и каждый
    // следующий кусок ждёт по полминуты: устройство перестаёт отвечать брокеру и
    // пропадает с портала. Лучше оборвать передачу сразу — клиент повторит
    // запрос, когда связь станет лучше.
    const uint32_t freeHeap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    if (freeHeap < kMinHeapForConfig) {
        HAL_LOG_WARN("MENU", "config transfer aborted: low heap (%u bytes free)",
                     (unsigned)freeHeap);
        return 0;
    }
#endif

    // В локальный WS отдаём те же куски: клиент в LAN собирает их так же, как
    // портал.
    wsPublishRaw("config", json, len);


    const uint16_t id = mqtt_->publishConfigChunk(json, len, first);


    // Каналы независимы, а отправитель меню прекращает передачу по нулю. Пока
    // возвращали ответ брокера, отказ MQTT на середине обрывал и локального
    // клиента: телефон в той же сети получал 11 кусков из 28 и выбрасывал
    // недособранное. Локальный клиент на связи — кусок доставлен, идём дальше;
    // неудачу MQTT разбираем по его собственному логу.
    if (id == 0 && isLocalConnected()) return 1;
    return id;
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

bool DevicePublisher::publishRfid(JsonDocument& doc) {
    wsPublish("rfid", doc);
    return mqtt_->publishRfid(doc);
}

bool DevicePublisher::publishWeights(JsonDocument& doc) {
    wsPublish("weights", doc);
    return mqtt_->publishWeights(doc);
}

bool DevicePublisher::publishRfidWriteResult(JsonDocument& doc) {
    // Local WS этот ответ не нужен — это специфичная backend-корреляция.
    return mqtt_->publishRfidWriteResult(doc);
}

} // namespace idryer
