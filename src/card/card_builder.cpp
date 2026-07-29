#include "card_builder.h"

#include <string.h>
#include <stdio.h>

namespace idryer {

// ── helpers ──────────────────────────────────────────────────────────────────

static void copyStr(char* dst, size_t dstSize, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

CardBuilder::Entity* CardBuilder::alloc_(Kind kind, const char* id, const char* label) {
    if (!id || !id[0]) return nullptr;
    // Повторная регистрация того же id — замена (как в onCommand/HaBuilder).
    for (uint8_t i = 0; i < count_; ++i) {
        if (strcmp(entities_[i].id, id) == 0) {
            entities_[i] = Entity{};
            entities_[i].kind = kind;
            copyStr(entities_[i].id, sizeof(Entity::id), id);
            copyStr(entities_[i].label, sizeof(Entity::label), label);
            dirty_ = true;
            return &entities_[i];
        }
    }
    if (count_ >= MAX_ENTITIES) return nullptr;
    Entity& e = entities_[count_++];
    e = Entity{};
    e.kind = kind;
    copyStr(e.id, sizeof(Entity::id), id);
    copyStr(e.label, sizeof(Entity::label), label);
    dirty_ = true;
    return &e;
}

// ── declaration API ──────────────────────────────────────────────────────────

bool CardBuilder::sensor(const char* id, const char* label, const char* unit,
                         const char* path, const char* deviceClass) {
    Entity* e = alloc_(SensorK, id, label);
    if (!e) return false;
    copyStr(e->unit, sizeof(Entity::unit), unit);
    copyStr(e->path, sizeof(Entity::path), path);
    copyStr(e->deviceClass, sizeof(Entity::deviceClass), deviceClass);
    return true;
}

bool CardBuilder::binarySensor(const char* id, const char* label, const char* path) {
    Entity* e = alloc_(BinaryK, id, label);
    if (!e) return false;
    copyStr(e->path, sizeof(Entity::path), path);
    return true;
}

bool CardBuilder::button(const char* id, const char* label, OnPress cb) {
    Entity* e = alloc_(ButtonK, id, label);
    if (!e) return false;
    e->onPress = cb;
    return true;
}

bool CardBuilder::number(const char* id, const char* label, float min, float max,
                         float step, const char* unit, OnNumber cb) {
    Entity* e = alloc_(NumberK, id, label);
    if (!e) return false;
    e->numMin = min; e->numMax = max; e->numStep = step;
    copyStr(e->unit, sizeof(Entity::unit), unit);
    e->onNumber = cb;
    return true;
}

bool CardBuilder::select(const char* id, const char* label,
                         const char* const* options, uint8_t count, OnSelect cb) {
    Entity* e = alloc_(SelectK, id, label);
    if (!e) return false;
    e->optCount = count < Entity::MAX_OPTS ? count : Entity::MAX_OPTS;
    for (uint8_t i = 0; i < e->optCount; ++i) {
        copyStr(e->options[i], Entity::OPT_LEN + 1, options[i]);
    }
    e->onSelect = cb;
    return true;
}

bool CardBuilder::layoutRow(const char* a, const char* b, const char* c, const char* d) {
    if (layoutRows_ >= MAX_ROWS || !a) return false;
    const char* ids[MAX_ROW_ITEMS] = { a, b, c, d };
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_ROW_ITEMS && ids[i]; ++i) {
        copyStr(layout_[layoutRows_][n++], sizeof(layout_[0][0]), ids[i]);
    }
    rowLen_[layoutRows_++] = n;
    dirty_ = true;
    return true;
}

// ── manifest JSON ────────────────────────────────────────────────────────────

// Авто-сенсор из capability_vocabulary: без label (портал переводит по
// device_class сам, как для своих продуктов).
static void addAutoSensor(JsonArray& arr, const char* id, const char* deviceClass,
                          const char* unit, const char* path, bool binary = false) {
    JsonObject e = arr.createNestedObject();
    e["id"]           = id;
    e["type"]         = binary ? "binary_sensor" : "sensor";
    e["device_class"] = deviceClass;
    if (unit && unit[0]) e["unit"] = unit;
    e["source"] = "telemetry";
    e["path"]   = path;
}

void CardBuilder::buildJson(JsonDocument& doc, const iDryer::Config& cfg) const {
    doc["v"] = 1;
    JsonArray arr = doc.createNestedArray("entities");

    // ── Авто-сенсоры из Config.has* (пути — как publishTelemetryNow) ──
    // MVP: unit 0. Multi-unit манифест — после нормализации на портале.
    if (cfg.hasAirTemp)
        addAutoSensor(arr, "temp", "temperature", "°C", "units[0].temperature");
    if (cfg.hasAirHumidity)
        addAutoSensor(arr, "humidity", "humidity", "%", "units[0].humidity");
    if (cfg.hasHeaterTemp)
        addAutoSensor(arr, "heater_temp", "heater_temp", "°C", "units[0].heaterTemp");
    if (cfg.hasHeater)
        addAutoSensor(arr, "power", "power", "%", "units[0].heaterPower");
    if (cfg.hasFan)
        addAutoSensor(arr, "fan", "fan", nullptr, "units[0].fanStatus", /*binary=*/true);
    if (cfg.hasServo)
        addAutoSensor(arr, "servo", "servo", nullptr, "units[0].servoOpen", /*binary=*/true);

    // ── Объявленные продуктом сущности ──
    for (uint8_t i = 0; i < count_; ++i) {
        const Entity& src = entities_[i];
        JsonObject e = arr.createNestedObject();
        e["id"] = src.id;
        switch (src.kind) {
            case SensorK:
                e["type"] = "sensor";
                if (src.deviceClass[0]) e["device_class"] = src.deviceClass;
                if (src.unit[0])        e["unit"] = src.unit;
                e["source"] = "telemetry";
                e["path"]   = src.path;
                break;
            case BinaryK:
                e["type"]   = "binary_sensor";
                e["source"] = "telemetry";
                e["path"]   = src.path;
                break;
            case ButtonK:
                e["type"] = "button";
                break;
            case NumberK:
                e["type"] = "number";
                e["min"]  = src.numMin;
                e["max"]  = src.numMax;
                e["step"] = src.numStep;
                if (src.unit[0]) e["unit"] = src.unit;
                break;
            case SelectK: {
                e["type"] = "select";
                JsonArray opts = e.createNestedArray("options");
                for (uint8_t k = 0; k < src.optCount; ++k) opts.add(src.options[k]);
                break;
            }
        }
        if (src.label[0]) e["label"] = src.label;
        if (src.kind == ButtonK || src.kind == NumberK || src.kind == SelectK) {
            char action[32];
            snprintf(action, sizeof(action), "card.%s", src.id);
            e["action"] = action;
            if (src.kind != ButtonK) e["arg"] = "value";
        }
    }

    // ── Заводская разметка (слой 2) ──
    if (layoutRows_ > 0) {
        JsonArray layout = doc.createNestedArray("layout");
        for (uint8_t r = 0; r < layoutRows_; ++r) {
            JsonArray row = layout.createNestedArray();
            for (uint8_t c = 0; c < rowLen_[r]; ++c) row.add(layout_[r][c]);
        }
    }
}

// ── invoke routing ───────────────────────────────────────────────────────────

bool CardBuilder::handleInvokeAction(const char* action, JsonObjectConst args) {
    if (!action || strncmp(action, "card.", 5) != 0) return false;
    const char* id = action + 5;
    for (uint8_t i = 0; i < count_; ++i) {
        Entity& e = entities_[i];
        if (strcmp(e.id, id) != 0) continue;
        switch (e.kind) {
            case ButtonK:
                if (e.onPress) e.onPress();
                return true;
            case NumberK:
                if (e.onNumber && args["value"].is<float>()) {
                    float v = args["value"].as<float>();
                    if (v < e.numMin) v = e.numMin;
                    if (v > e.numMax) v = e.numMax;
                    e.onNumber(v);
                }
                return true;
            case SelectK:
                if (e.onSelect && args["value"].is<const char*>()) {
                    const char* opt = args["value"].as<const char*>();
                    // Только объявленные опции — чужие строки в колбэк не идут.
                    for (uint8_t k = 0; k < e.optCount; ++k) {
                        if (strcmp(e.options[k], opt) == 0) { e.onSelect(opt); break; }
                    }
                }
                return true;
            default:
                return false; // sensor/binary_sensor не принимают команды
        }
    }
    return false;
}

} // namespace idryer
