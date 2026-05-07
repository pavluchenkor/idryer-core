#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_builder.h"
#include "../../hal/hal_types.h"

#include <Arduino.h>
#include <string.h>
#include <stdlib.h>

namespace idryer {
namespace ha {

namespace {
constexpr const char* HA_PREFIX = "homeassistant";

void copyStr(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}
} // namespace

HaBuilder::HaBuilder(HaPublisher* pub, HaMqttClient* mqtt)
    : pub_(pub), mqtt_(mqtt) {}

void HaBuilder::setDeviceId(const char* deviceId) {
    copyStr(deviceId_, sizeof(deviceId_), deviceId);
}

void HaBuilder::buildCmdTopic(const Entry& e, char* out, size_t outSize) {
    snprintf(out, outSize, "idryer_ha/%s/%s/cmd", deviceId_, e.id);
}

void HaBuilder::button(const char* id, const char* name, OnPress cb,
                       const char* icon) {
    if (count_ >= MAX_ENTRIES || !id || !name) return;
    Entry& e = entries_[count_++];
    e.kind = ButtonK;
    copyStr(e.id,   sizeof(e.id),   id);
    copyStr(e.name, sizeof(e.name), name);
    copyStr(e.icon, sizeof(e.icon), icon);
    e.onPress = std::move(cb);
}

void HaBuilder::number(const char* id, const char* name, int min, int max,
                       OnNumber cb, const char* unit, const char* icon, int step) {
    if (count_ >= MAX_ENTRIES || !id || !name) return;
    Entry& e = entries_[count_++];
    e.kind = NumberK;
    copyStr(e.id,   sizeof(e.id),   id);
    copyStr(e.name, sizeof(e.name), name);
    copyStr(e.icon, sizeof(e.icon), icon);
    copyStr(e.unit, sizeof(e.unit), unit);
    e.numMin = min; e.numMax = max; e.numStep = step;
    e.onNumber = std::move(cb);
}

void HaBuilder::select(const char* id, const char* name,
                       const char* const* options, uint8_t optCount,
                       OnSelect cb, const char* icon) {
    if (count_ >= MAX_ENTRIES || !id || !name || !options || optCount == 0) return;
    Entry& e = entries_[count_++];
    e.kind = SelectK;
    copyStr(e.id,   sizeof(e.id),   id);
    copyStr(e.name, sizeof(e.name), name);
    copyStr(e.icon, sizeof(e.icon), icon);
    uint8_t n = (optCount > Entry::MAX_OPTS) ? Entry::MAX_OPTS : optCount;
    for (uint8_t i = 0; i < n; ++i) copyStr(e.options[i], Entry::OPT_LEN, options[i]);
    e.optCount = n;
    e.onSelect = std::move(cb);
}

void HaBuilder::publishOne(const Entry& e) {
    if (!mqtt_ || !mqtt_->isConnected() || deviceId_[0] == '\0') return;

    const char* domain = "sensor";
    switch (e.kind) {
        case ButtonK: domain = "button"; break;
        case NumberK: domain = "number"; break;
        case SelectK: domain = "select"; break;
    }
    char configTopic[140];
    snprintf(configTopic, sizeof(configTopic),
             "%s/%s/idryer_%s_%s/config",
             HA_PREFIX, domain, deviceId_, e.id);

    char cmdTopic[100];
    buildCmdTopic(e, cmdTopic, sizeof(cmdTopic));

    char uniqueId[80];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s", deviceId_, e.id);

    // Минимальный device-блок — HA сгруппирует все entities одного устройства.
    char deviceJson[160];
    snprintf(deviceJson, sizeof(deviceJson),
             "\"device\":{\"identifiers\":[\"idryer_%s\"],"
             "\"name\":\"iDryer %s\",\"manufacturer\":\"iDryer\"}",
             deviceId_, deviceId_);

    char payload[600];
    if (e.kind == ButtonK) {
        snprintf(payload, sizeof(payload),
                 "{\"name\":\"%s\",\"unique_id\":\"%s\",\"command_topic\":\"%s\","
                 "%s%s%s%s}",
                 e.name, uniqueId, cmdTopic,
                 e.icon[0] ? "\"icon\":\"" : "", e.icon[0] ? e.icon : "",
                 e.icon[0] ? "\"," : "", deviceJson);
    } else if (e.kind == NumberK) {
        snprintf(payload, sizeof(payload),
                 "{\"name\":\"%s\",\"unique_id\":\"%s\",\"command_topic\":\"%s\","
                 "\"min\":%d,\"max\":%d,\"step\":%d,"
                 "%s%s%s"
                 "%s%s%s%s}",
                 e.name, uniqueId, cmdTopic,
                 e.numMin, e.numMax, e.numStep,
                 e.unit[0] ? "\"unit_of_measurement\":\"" : "", e.unit[0] ? e.unit : "",
                 e.unit[0] ? "\"," : "",
                 e.icon[0] ? "\"icon\":\"" : "", e.icon[0] ? e.icon : "",
                 e.icon[0] ? "\"," : "", deviceJson);
    } else { // SelectK
        char opts[200] = "[";
        for (uint8_t i = 0; i < e.optCount; ++i) {
            strncat(opts, "\"", sizeof(opts) - strlen(opts) - 1);
            strncat(opts, e.options[i], sizeof(opts) - strlen(opts) - 1);
            strncat(opts, (i + 1 < e.optCount) ? "\"," : "\"",
                    sizeof(opts) - strlen(opts) - 1);
        }
        strncat(opts, "]", sizeof(opts) - strlen(opts) - 1);

        snprintf(payload, sizeof(payload),
                 "{\"name\":\"%s\",\"unique_id\":\"%s\",\"command_topic\":\"%s\","
                 "\"options\":%s,"
                 "%s%s%s%s}",
                 e.name, uniqueId, cmdTopic, opts,
                 e.icon[0] ? "\"icon\":\"" : "", e.icon[0] ? e.icon : "",
                 e.icon[0] ? "\"," : "", deviceJson);
    }

    mqtt_->publish(configTopic, payload, /*retained=*/true);
    mqtt_->subscribe(cmdTopic);
}

void HaBuilder::republishAll() {
    for (uint8_t i = 0; i < count_; ++i) publishOne(entries_[i]);
    if (count_ > 0) {
        HAL_LOG_INFO("HA_BUILDER", "Published %d HA controls for %s", count_, deviceId_);
    }
}

void HaBuilder::handleIncoming(const char* topic, const char* payload) {
    if (!topic || !payload) return;
    char prefix[80];
    snprintf(prefix, sizeof(prefix), "idryer_ha/%s/", deviceId_);
    size_t plen = strlen(prefix);
    if (strncmp(topic, prefix, plen) != 0) return;

    const char* rest = topic + plen;
    const char* slash = strchr(rest, '/');
    if (!slash) return;
    size_t idLen = slash - rest;
    if (idLen >= sizeof(entries_[0].id)) return;

    char id[24];
    memcpy(id, rest, idLen);
    id[idLen] = '\0';

    for (uint8_t i = 0; i < count_; ++i) {
        if (strcmp(entries_[i].id, id) != 0) continue;
        const Entry& e = entries_[i];
        if (e.kind == ButtonK && e.onPress)  e.onPress();
        else if (e.kind == NumberK && e.onNumber) e.onNumber(atoi(payload));
        else if (e.kind == SelectK && e.onSelect) e.onSelect(payload);
        return;
    }
}

} // namespace ha
} // namespace idryer

#endif // ESP32
