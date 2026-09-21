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

// ── actions (v2) ─────────────────────────────────────────────────────────────

CardBuilder::ActionRef CardBuilder::action(const char* id, const char* mode, OnAction cb) {
    if (!id || !id[0] || !mode || !mode[0]) return ActionRef(this, -1);
    int8_t idx = -1;
    // Повторная регистрация того же id — замена, как у сущностей.
    for (uint8_t i = 0; i < actionCount_; ++i) {
        if (strcmp(actions_[i].id, id) == 0) { idx = (int8_t)i; break; }
    }
    if (idx < 0) {
        if (actionCount_ >= MAX_ACTIONS) return ActionRef(this, -1);
        idx = (int8_t)actionCount_++;
    }
    Action& a = actions_[idx];
    a = Action{};
    copyStr(a.id, sizeof(Action::id), id);
    copyStr(a.mode, sizeof(Action::mode), mode);
    a.cb = cb;
    dirty_ = true;
    return ActionRef(this, idx);
}

CardBuilder::Param* CardBuilder::addParam_(int8_t actionIdx, const char* id, const char* purpose) {
    if (actionIdx < 0 || actionIdx >= (int8_t)actionCount_ || !id || !id[0]) return nullptr;
    Action& a = actions_[actionIdx];
    if (a.paramCount >= MAX_PARAMS) return nullptr;
    Param& p = a.params[a.paramCount++];
    p = Param{};
    copyStr(p.id, sizeof(Param::id), id);
    copyStr(p.purpose, sizeof(Param::purpose), purpose);
    dirty_ = true;
    return &p;
}

CardBuilder::ActionRef& CardBuilder::ActionRef::param(const char* id, const char* purpose,
                                                      uint16_t menuId) {
    if (Param* p = b_->addParam_(idx_, id, purpose)) p->menuId = menuId;
    return *this;
}

CardBuilder::ActionRef& CardBuilder::ActionRef::param(const char* id, const char* purpose,
                                                      float min, float max, float step,
                                                      float def, const char* unit) {
    if (Param* p = b_->addParam_(idx_, id, purpose)) {
        p->min = min; p->max = max; p->step = step; p->def = def;
        copyStr(p->unit, sizeof(Param::unit), unit);
    }
    return *this;
}

CardBuilder::ActionRef& CardBuilder::ActionRef::ceiling(uint16_t menuId) {
    if (idx_ < 0) return *this;
    Action& a = b_->actions_[idx_];
    if (a.paramCount > 0) a.params[a.paramCount - 1].ceilId = menuId;
    return *this;
}

CardBuilder::ActionRef& CardBuilder::ActionRef::stages(const char* id, const char* purpose) {
    if (Param* p = b_->addParam_(idx_, id, purpose)) p->type = ParamStages;
    return *this;
}

bool CardBuilder::resolve_(const Param& p, uint8_t unit, float& mn, float& mx,
                           float& st, float& df, const char** unitStr) const {
    mn = p.min; mx = p.max; st = p.step; df = p.def;
    if (unitStr) *unitStr = p.unit[0] ? p.unit : nullptr;
    if (p.menuId != NO_MENU) {
        MenuValue v;
        if (!menuReader_ || !menuReader_(p.menuId, unit, v)) return false;
        mn = v.min; mx = v.max; st = v.step; df = v.value;
        if (unitStr && !p.unit[0]) *unitStr = v.unit;
    }
    if (p.ceilId != NO_MENU) {
        MenuValue c;
        if (menuReader_ && menuReader_(p.ceilId, unit, c) && c.value < mx) mx = c.value;
    }
    if (mx < mn) mx = mn;
    if (df < mn) df = mn;
    if (df > mx) df = mx;
    return true;
}

uint32_t CardBuilder::fingerprint_(uint8_t unitsCount) const {
    // FNV-1a по всему, что параметры берут из меню, плюс число юнитов.
    uint32_t h = 2166136261u;
    auto mix = [&h](const void* data, size_t len) {
        const uint8_t* b = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i) { h ^= b[i]; h *= 16777619u; }
    };
    mix(&unitsCount, sizeof(unitsCount));
    for (uint8_t i = 0; i < actionCount_; ++i) {
        const Action& a = actions_[i];
        for (uint8_t k = 0; k < a.paramCount; ++k) {
            const Param& p = a.params[k];
            if (p.type != ParamNumber || (p.menuId == NO_MENU && p.ceilId == NO_MENU)) continue;
            for (uint8_t u = 0; u < unitsCount; ++u) {
                float v[4];
                const bool ok = resolve_(p, u, v[0], v[1], v[2], v[3], nullptr);
                mix(&ok, sizeof(ok));
                mix(v, sizeof(v));
            }
        }
    }
    return h;
}

void CardBuilder::pollMenu(uint8_t unitsCount) {
    if (actionCount_ == 0) return;
    const uint32_t fp = fingerprint_(unitsCount);
    if (fp != menuFingerprint_) {
        menuFingerprint_ = fp;
        dirty_ = true;
    }
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

    // ── Действия (v2) ──
    if (actionCount_ == 0) return;
    doc["v"] = 2;
    const uint8_t units = cfg.unitsCount > 0 && cfg.unitsCount <= iDryer::MAX_UNITS
                              ? cfg.unitsCount : 1;
    JsonArray acts = doc.createNestedArray("actions");
    for (uint8_t i = 0; i < actionCount_; ++i) {
        const Action& a = actions_[i];
        JsonObject o = acts.createNestedObject();
        o["id"]   = a.id;
        o["mode"] = a.mode;
        char action[32];
        snprintf(action, sizeof(action), "card.%s", a.id);
        o["action"] = action;
        if (a.paramCount == 0) continue;

        JsonArray params = o.createNestedArray("params");
        for (uint8_t k = 0; k < a.paramCount; ++k) {
            const Param& p = a.params[k];
            if (p.type == ParamStages) {
                JsonObject po = params.createNestedObject();
                po["id"] = p.id;
                if (p.purpose[0]) po["purpose"] = p.purpose;
                po["type"] = "stages";
                continue;
            }
            // Пределы и значения по юнитам. Совпали у всех — скаляром.
            float mn[iDryer::MAX_UNITS], mx[iDryer::MAX_UNITS], df[iDryer::MAX_UNITS];
            float st = p.step;
            const char* unitStr = nullptr;
            bool ok = true;
            for (uint8_t u = 0; u < units && ok; ++u) {
                ok = resolve_(p, u, mn[u], mx[u], st, df[u], &unitStr);
            }
            // Меню ещё не прочитано — параметр без пределов не публикуем.
            if (!ok) continue;

            bool sameLimits = true, sameDefault = true;
            for (uint8_t u = 1; u < units; ++u) {
                if (mn[u] != mn[0] || mx[u] != mx[0]) sameLimits = false;
                if (df[u] != df[0]) sameDefault = false;
            }

            JsonObject po = params.createNestedObject();
            po["id"] = p.id;
            if (p.purpose[0]) po["purpose"] = p.purpose;
            po["type"] = "number";
            JsonArray lim = po.createNestedArray("limits");
            if (sameLimits) {
                lim.add(mn[0]); lim.add(mx[0]);
            } else {
                for (uint8_t u = 0; u < units; ++u) {
                    JsonArray pair = lim.createNestedArray();
                    pair.add(mn[u]); pair.add(mx[u]);
                }
            }
            po["step"] = st;
            if (sameDefault) {
                po["default"] = df[0];
            } else {
                JsonArray d = po.createNestedArray("default");
                for (uint8_t u = 0; u < units; ++u) d.add(df[u]);
            }
            if (unitStr && unitStr[0]) po["unit"] = unitStr;
        }
    }
}

// ── invoke routing ───────────────────────────────────────────────────────────

bool CardBuilder::handleInvokeAction(const char* action, JsonObjectConst data,
                                     uint8_t unitsCount) {
    if (!action || strncmp(action, "card.", 5) != 0) return false;
    const char* id = action + 5;
    JsonObjectConst args = data["args"];

    // ── Действия ──
    for (uint8_t i = 0; i < actionCount_; ++i) {
        const Action& a = actions_[i];
        if (strcmp(a.id, id) != 0) continue;

        // unitId "U1".."U4" → 0..3; без него — первый юнит.
        uint8_t unit = 0;
        const char* uid = data["unitId"] | "";
        if (uid[0] == 'U' && uid[1] >= '1' && uid[1] <= '9' && uid[2] == '\0') {
            unit = (uint8_t)(uid[1] - '1');
        }
        const uint8_t units = unitsCount > 0 ? unitsCount : 1;
        if (unit >= units) return true;   // чужой юнит — команду не исполняем
        if (!a.cb) return true;

        // Числа — в пределах юнита, пропущенные — значением по умолчанию.
        // Стадии передаются как пришли: их сверяет продукт при сборке команды.
        DynamicJsonDocument out(2048);
        for (uint8_t k = 0; k < a.paramCount; ++k) {
            const Param& p = a.params[k];
            if (p.type == ParamStages) {
                out[p.id] = args[p.id];
                continue;
            }
            float mn, mx, st, df;
            if (!resolve_(p, unit, mn, mx, st, df, nullptr)) return true;
            float v = args[p.id].is<float>() ? args[p.id].as<float>() : df;
            if (v < mn) v = mn;
            if (v > mx) v = mx;
            out[p.id] = v;
        }
        a.cb(unit, out.as<JsonObjectConst>());
        return true;
    }

    // ── Контролы-сущности ──
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
