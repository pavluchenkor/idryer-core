#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_card_projection.h"
#include "../../hal/hal_types.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace idryer {
namespace ha {

namespace {

constexpr const char* kComp[] = { "sensor", "binary_sensor", "number", "select", "text", "button" };

void copyStr(char* dst, size_t n, const char* src) {
    strncpy(dst, src ? src : "", n - 1);
    dst[n - 1] = '\0';
}

// id → object_id HA: строчные латинские, цифры, подчёркивания.
void sanitize(char* s) {
    for (; *s; ++s) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') *s = (char)(c - 'A' + 'a');
        else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) *s = '_';
    }
}

// «heater_temp» → «Heater temp».
void pretty(char* out, size_t n, const char* id) {
    copyStr(out, n, id);
    for (char* p = out; *p; ++p) if (*p == '_') *p = ' ';
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
}

// Названия в HA — только английские: язык HA прибору неизвестен, а из
// латиницы HA собирает читаемые entity_id.
const char* sensorName(const char* dc) {
    if (!strcmp(dc, "temperature")) return "Temperature";
    if (!strcmp(dc, "humidity"))    return "Humidity";
    if (!strcmp(dc, "heater_temp")) return "Heater temperature";
    if (!strcmp(dc, "power"))       return "Heater power";
    if (!strcmp(dc, "fan"))         return "Fan";
    if (!strcmp(dc, "servo"))       return "Damper";
    if (!strcmp(dc, "weight"))      return "Weight";
    return nullptr;
}

// Класс карточки → device_class HA. Неизвестный не передаётся: HA отвергает
// сущность с неподходящим классом.
const char* haSensorClass(const char* dc) {
    if (!strcmp(dc, "temperature") || !strcmp(dc, "heater_temp")) return "temperature";
    if (!strcmp(dc, "humidity")) return "humidity";
    if (!strcmp(dc, "power"))    return "power_factor";
    if (!strcmp(dc, "weight"))   return "weight";
    return nullptr;
}

const char* haBinaryClass(const char* dc) {
    if (!strcmp(dc, "fan"))   return "running";
    if (!strcmp(dc, "servo")) return "opening";
    return nullptr;
}

const char* purposeName(const char* purpose) {
    if (!strcmp(purpose, "target_temperature")) return "temperature";
    if (!strcmp(purpose, "target_humidity"))    return "humidity";
    if (!strcmp(purpose, "duration"))           return "duration";
    if (!strcmp(purpose, "effect"))             return "effect";
    if (!strcmp(purpose, "rgb_color"))          return "color";
    return nullptr;
}

bool isHexColor(const char* s) {
    if (!s || s[0] != '#' || strlen(s) != 7) return false;
    for (int i = 1; i < 7; ++i) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

// Пара limits / default для юнита: скаляр — общий, массив — по юниту.
void unitLimits(JsonVariantConst lim, uint8_t unit, float& mn, float& mx) {
    JsonArrayConst a = lim.as<JsonArrayConst>();
    if (a.isNull() || a.size() < 2) return;
    if (a[0].is<JsonArrayConst>()) {
        JsonArrayConst pair = a[unit < a.size() ? unit : 0].as<JsonArrayConst>();
        mn = pair[0] | mn;
        mx = pair[1] | mx;
    } else {
        mn = a[0] | mn;
        mx = a[1] | mx;
    }
}

float unitDefault(JsonVariantConst def, uint8_t unit, float fallback) {
    if (def.is<JsonArrayConst>()) {
        JsonArrayConst a = def.as<JsonArrayConst>();
        if (a.size() == 0) return fallback;
        return a[unit < a.size() ? unit : 0] | fallback;
    }
    return def | fallback;
}

} // namespace

// ── Идентичность и соединение ───────────────────────────────────────────────

void HaCardProjection::setDevice(const char* serial, const char* model,
                                 const char* fw, const char* hw) {
    copyStr(serial_, sizeof(serial_), serial);
    copyStr(model_, sizeof(model_), model && model[0] ? model : "iDryer");
    copyStr(fw_, sizeof(fw_), fw);
    copyStr(hw_, sizeof(hw_), hw);
    // Оборвалось соединение — брокер сам скажет HA, что прибор недоступен.
    char avty[80];
    snprintf(avty, sizeof(avty), "idryer/%s/ha/availability", serial_);
    if (mqtt_) mqtt_->setWill(avty, "offline");
}

void HaCardProjection::setLegacyId(const char* id) {
    if (id && strcmp(id, serial_) != 0) copyStr(legacyId_, sizeof(legacyId_), id);
}

void HaCardProjection::onConnected() {
    connected_  = true;
    subscribed_ = false;
    requestRepublish();
}

void HaCardProjection::onDisconnected() {
    connected_      = false;
    subscribed_     = false;
    cleanupUntilMs_ = 0;
    pendingCount_   = 0;
    staleCount_     = 0;
    legacyStep_     = 0xFF;
    if (doc_) { delete doc_; doc_ = nullptr; }
    phase_ = PhIdle;
}

void HaCardProjection::requestRepublish() {
    needPublish_    = true;
    publishAfterMs_ = millis() + SETTLE_MS;
}

void HaCardProjection::topic_(char* out, size_t n, const char* object, const char* leaf) const {
    snprintf(out, n, "idryer/%s/ha/%s/%s", serial_, object, leaf);
}

// ── Проход публикации ───────────────────────────────────────────────────────

void HaCardProjection::startPass_() {
    needPublish_ = false;
    if (!manifestFn_ || !serial_[0]) return;
    doc_ = new DynamicJsonDocument(4096);
    if (!doc_ || doc_->capacity() == 0) {
        delete doc_; doc_ = nullptr;
        needPublish_ = true;   // повтор позже: сейчас не хватило памяти
        return;
    }
    const uint8_t units = manifestFn_(manifestCtx_, *doc_);
    if (doc_->overflowed()) {
        HAL_LOG_ERROR("HA_CARD", "manifest overflow");
        delete doc_; doc_ = nullptr;
        return;
    }
    units_ = units < 1 ? 1 : (units > 4 ? 4 : units);
    hasModes_ = false;
    for (JsonObjectConst a : (*doc_)["actions"].as<JsonArrayConst>()) {
        if (a["mode"].is<const char*>()) { hasModes_ = true; break; }
    }
    for (uint8_t i = 0; i < publishedCount_; ++i) published_[i].seen = false;
    for (uint8_t i = 0; i < controlCount_; ++i) controls_[i].live = false;

    char avty[80];
    snprintf(avty, sizeof(avty), "idryer/%s/ha/availability", serial_);
    mqtt_->publish(avty, "online", true);

    phase_ = PhEntities;
    ci_ = cu_ = ck_ = 0;
}

// Одна публикация за вызов; false — проход закончен.
bool HaCardProjection::emitNext_() {
    JsonObjectConst m = doc_->as<JsonObjectConst>();
    while (true) {
        if (phase_ == PhEntities) {
            JsonArrayConst ents = m["entities"].as<JsonArrayConst>();
            if (ci_ >= ents.size()) { phase_ = PhModes; cu_ = 0; continue; }
            JsonObjectConst e = ents[ci_].as<JsonObjectConst>();
            if (cu_ >= entityItems_(e)) { ++ci_; cu_ = 0; continue; }
            emitEntity_(e, cu_++);
            return true;
        }
        if (phase_ == PhModes) {
            if (!hasModes_ || cu_ >= units_) { phase_ = PhActions; ci_ = cu_ = ck_ = 0; continue; }
            emitMode_(cu_++);
            return true;
        }
        if (phase_ == PhActions) {
            JsonArrayConst acts = m["actions"].as<JsonArrayConst>();
            if (ci_ >= acts.size()) return false;
            JsonObjectConst a = acts[ci_].as<JsonObjectConst>();
            // Стадии профиля берутся из профилей портала — в HA их нет.
            bool stages = false;
            for (JsonObjectConst p : a["params"].as<JsonArrayConst>()) {
                if (!strcmp(p["type"] | "", "stages")) { stages = true; break; }
            }
            if (stages || !(a["id"] | "")[0] || cu_ >= units_) { ++ci_; cu_ = ck_ = 0; continue; }
            JsonArrayConst params = a["params"].as<JsonArrayConst>();
            if (ck_ < params.size()) {
                const bool sent = emitParam_(a, params[ck_++].as<JsonObjectConst>(), cu_);
                if (sent) return true;
                continue;
            }
            emitButton_(a, cu_);
            ++cu_; ck_ = 0;
            return true;
        }
        return false;
    }
}

void HaCardProjection::finishPass_() {
    delete doc_; doc_ = nullptr;
    phase_ = PhIdle;

    // Контролы, которых в манифесте больше нет, — из таблицы.
    uint8_t w = 0;
    for (uint8_t i = 0; i < controlCount_; ++i) {
        if (!controls_[i].live) continue;
        if (w != i) controls_[w] = controls_[i];
        const Kind k = controls_[w].kind;
        controls_[w].stateDirty = (k == ParamNumber || k == ParamSelect || k == ParamColor);
        ++w;
    }
    controlCount_ = w;
    pendingCount_ = 0;   // индексы могли сдвинуться

    // Сущности, которых больше нет, — в очередь на удаление из HA.
    w = 0;
    for (uint8_t i = 0; i < publishedCount_; ++i) {
        if (published_[i].seen) {
            if (w != i) published_[w] = published_[i];
            ++w;
        } else {
            queueStale_(published_[i].object, published_[i].comp);
        }
    }
    publishedCount_ = w;

    if (!subscribed_) {
        subscribed_ = true;
        char filter[96];
        snprintf(filter, sizeof(filter), "idryer/%s/ha/+/set", serial_);
        mqtt_->subscribe(filter);
        // Окно уборки: свои retained-конфиги, которых нет в наборе, — удалить
        // по его окончании. Подписка только на свой node_id: конфиги всех
        // устройств HA не нужны.
        snprintf(filter, sizeof(filter), "homeassistant/+/idryer_%s/+/config", serial_);
        mqtt_->subscribe(filter);
        cleanupUntilMs_ = millis() + CLEANUP_MS;
        if (cleanupUntilMs_ == 0) cleanupUntilMs_ = 1;
        legacyStep_ = 0;
    }
    HAL_LOG_INFO("HA_CARD", "discovery: %u entities, %u controls", publishedCount_, controlCount_);
    if (publishedFn_) publishedFn_(publishedCtx_);
}

// ── Элементы discovery ──────────────────────────────────────────────────────

// Сколько сущностей HA даёт сущность манифеста.
uint8_t HaCardProjection::entityItems_(JsonObjectConst e) const {
    const char* type = e["type"] | "";
    if (!(e["id"] | "")[0]) return 0;
    if (!strcmp(type, "number") || !strcmp(type, "select") || !strcmp(type, "button")) return 1;
    if (strcmp(type, "sensor") != 0 && strcmp(type, "binary_sensor") != 0) return 0;
    const char* path = e["path"] | "";
    const bool weights = !strcmp(e["source"] | "telemetry", "weights");
    if (!weights && !path[0]) return 0;
    // Путь units[0].* — поле юнита: сущность на каждый юнит.
    return (weights || !strncmp(path, "units[0]", 8)) ? units_ : 1;
}

void HaCardProjection::emitEntity_(JsonObjectConst e, uint8_t u) {
    const char* type  = e["type"] | "";
    const char* rawId = e["id"] | "";
    char id[24];
    copyStr(id, sizeof(id), rawId);
    sanitize(id);
    const char* label = e["label"] | "";
    const char* dc    = e["device_class"] | "";
    StaticJsonDocument<1536> cfg;

    // Контролы-сущности: одна на устройство, команда — invoke card.{id} {value}.
    if (!strcmp(type, "number") || !strcmp(type, "select") || !strcmp(type, "button")) {
        char name[40];
        if (label[0]) copyStr(name, sizeof(name), label); else pretty(name, sizeof(name), rawId);
        cfg["name"] = name;
        char cmd[96];
        topic_(cmd, sizeof(cmd), id, "set");
        cfg["command_topic"] = cmd;
        Comp comp = CompButton;
        Kind kind = EntityButton;
        if (type[0] == 'n') {
            comp = CompNumber; kind = EntityNumber;
            cfg["min"]  = e["min"] | 0.0f;
            cfg["max"]  = e["max"] | 100.0f;
            cfg["step"] = e["step"] | 1.0f;
            cfg["mode"] = "box";
            if (e["unit"].is<const char*>()) cfg["unit_of_measurement"] = e["unit"];
            cfg["optimistic"] = true;
        } else if (type[0] == 's') {
            comp = CompSelect; kind = EntitySelect;
            cfg["options"] = e["options"];
            cfg["optimistic"] = true;
        } else {
            cfg["payload_press"] = "PRESS";
        }
        Control* c = upsertControl_(id, kind, 0, rawId, "");
        if (!c) return;
        c->min = e["min"] | 0.0f;
        c->max = e["max"] | 100.0f;
        sendConfig_(comp, id, cfg);
        return;
    }

    const bool binary  = type[0] == 'b';
    const char* path   = e["path"] | "";
    const char* source = e["source"] | "telemetry";
    const bool weights = !strcmp(source, "weights");
    const bool perUnit = weights || !strncmp(path, "units[0]", 8);

    char object[40];
    if (perUnit) snprintf(object, sizeof(object), "u%u_%s", (unsigned)(u + 1), id);
    else         copyStr(object, sizeof(object), id);

    char base[32];
    const char* known = sensorName(dc);
    if (label[0])   copyStr(base, sizeof(base), label);
    else if (known) copyStr(base, sizeof(base), known);
    else            pretty(base, sizeof(base), rawId);
    char name[48];
    if (perUnit && units_ > 1) snprintf(name, sizeof(name), "U%u %s", (unsigned)(u + 1), base);
    else                       copyStr(name, sizeof(name), base);

    char field[64];
    if (perUnit && !weights) snprintf(field, sizeof(field), "units[%u]%s", (unsigned)u, path + 8);
    else                     copyStr(field, sizeof(field), path);

    char tpl[160];
    if (weights) {
        snprintf(tpl, sizeof(tpl),
                 "{{ value_json.weights | selectattr('unitId', 'eq', 'U%u') "
                 "| map(attribute='value') | sum | round(1) }}", (unsigned)(u + 1));
    } else if (binary) {
        snprintf(tpl, sizeof(tpl), "{{ 'ON' if value_json.%s else 'OFF' }}", field);
    } else {
        snprintf(tpl, sizeof(tpl), "{{ value_json.%s | default(None) }}", field);
    }

    cfg["name"] = name;
    char st[64];
    snprintf(st, sizeof(st), "idryer/%s/%s", serial_, weights ? "weights" : source);
    cfg["state_topic"]    = st;
    cfg["value_template"] = tpl;
    if (binary) {
        if (const char* c = haBinaryClass(dc)) cfg["device_class"] = c;
        if (!strcmp(dc, "fan")) cfg["icon"] = "mdi:fan";
    } else {
        if (const char* c = haSensorClass(dc)) cfg["device_class"] = c;
        if (e["unit"].is<const char*>()) {
            cfg["unit_of_measurement"] = e["unit"];
            cfg["state_class"] = "measurement";
        }
        // Значения идут float — без точности HA покажет 28.60000038.
        if (!strcmp(dc, "power") || weights) cfg["suggested_display_precision"] = 0;
        else if (haSensorClass(dc))          cfg["suggested_display_precision"] = 1;
        if (!strcmp(dc, "power")) cfg["icon"] = "mdi:radiator";
    }
    sendConfig_(binary ? CompBinary : CompSensor, object, cfg);
}

// Режим юнита — из status: по нему видно, чем прибор занят.
void HaCardProjection::emitMode_(uint8_t u) {
    char object[16];
    snprintf(object, sizeof(object), "u%u_mode", (unsigned)(u + 1));
    char name[16];
    if (units_ > 1) snprintf(name, sizeof(name), "U%u Mode", (unsigned)(u + 1));
    else            copyStr(name, sizeof(name), "Mode");
    char st[64], tpl[80];
    snprintf(st, sizeof(st), "idryer/%s/status", serial_);
    snprintf(tpl, sizeof(tpl), "{{ value_json.units[%u].mode | default(None) }}", (unsigned)u);
    StaticJsonDocument<1536> cfg;
    cfg["name"] = name;
    cfg["state_topic"] = st;
    cfg["value_template"] = tpl;
    cfg["icon"] = "mdi:state-machine";
    sendConfig_(CompSensor, object, cfg);
}

static void actionName(JsonObjectConst a, char* out, size_t n) {
    const char* dc = a["device_class"] | "";
    if (a["name"]["en"].is<const char*>()) copyStr(out, n, a["name"]["en"]);
    else if (!strcmp(dc, "identify"))      copyStr(out, n, "Identify");
    else if (!strcmp(dc, "clear_errors"))  copyStr(out, n, "Clear errors");
    else                                   pretty(out, n, a["id"] | "");
}

// Поле параметра действия; false — тип параметра в HA не публикуется.
bool HaCardProjection::emitParam_(JsonObjectConst a, JsonObjectConst p, uint8_t u) {
    const char* ptype  = p["type"] | "";
    const char* rawPid = p["id"] | "";
    const char* rawAid = a["id"] | "";
    if (!rawPid[0]) return false;
    if (strcmp(ptype, "number") != 0 && strcmp(ptype, "select") != 0 && strcmp(ptype, "color") != 0) return false;
    char aid[24], pid[24];
    copyStr(aid, sizeof(aid), rawAid); sanitize(aid);
    copyStr(pid, sizeof(pid), rawPid); sanitize(pid);
    char object[40];
    snprintf(object, sizeof(object), "u%u_%s_%s", (unsigned)(u + 1), aid, pid);

    char aname[32];
    actionName(a, aname, sizeof(aname));
    char pname[24];
    if (const char* n = purposeName(p["purpose"] | "")) copyStr(pname, sizeof(pname), n);
    else { copyStr(pname, sizeof(pname), rawPid); for (char* c = pname; *c; ++c) if (*c == '_') *c = ' '; }
    char name[64];
    if (units_ > 1) snprintf(name, sizeof(name), "U%u %s %s", (unsigned)(u + 1), aname, pname);
    else            snprintf(name, sizeof(name), "%s %s", aname, pname);

    StaticJsonDocument<1536> cfg;
    cfg["name"] = name;
    char cmd[96], st[96];
    topic_(cmd, sizeof(cmd), object, "set");
    topic_(st, sizeof(st), object, "state");
    cfg["command_topic"] = cmd;
    cfg["state_topic"]   = st;
    const bool fresh = control_(object) == nullptr;

    if (ptype[0] == 'n') {
        float mn = 0, mx = 100;
        unitLimits(p["limits"], u, mn, mx);
        const float step = p["step"] | 1.0f;
        Control* c = upsertControl_(object, ParamNumber, u, rawAid, rawPid);
        if (!c) return false;
        c->min = mn; c->max = mx; c->step = step > 0 ? step : 1;
        if (fresh) c->num = unitDefault(p["default"], u, mn);
        if (c->num < mn) c->num = mn;
        if (c->num > mx) c->num = mx;
        cfg["min"]  = mn;
        cfg["max"]  = mx;
        cfg["step"] = c->step;
        cfg["mode"] = "box";
        if (p["unit"].is<const char*>()) cfg["unit_of_measurement"] = p["unit"];
        sendConfig_(CompNumber, object, cfg);
    } else if (ptype[0] == 's') {
        Control* c = upsertControl_(object, ParamSelect, u, rawAid, rawPid);
        if (!c) return false;
        if (fresh) copyStr(c->str, sizeof(c->str), p["default"] | (p["options"][0] | ""));
        cfg["options"] = p["options"];
        sendConfig_(CompSelect, object, cfg);
    } else {
        Control* c = upsertControl_(object, ParamColor, u, rawAid, rawPid);
        if (!c) return false;
        if (fresh) copyStr(c->str, sizeof(c->str), p["default"] | "#FFFFFF");
        cfg["pattern"] = "^#[0-9A-Fa-f]{6}$";
        cfg["min"] = 7;
        cfg["max"] = 7;
        sendConfig_(CompText, object, cfg);
    }
    return true;
}

void HaCardProjection::emitButton_(JsonObjectConst a, uint8_t u) {
    const char* rawAid = a["id"] | "";
    char aid[24];
    copyStr(aid, sizeof(aid), rawAid);
    sanitize(aid);
    char object[40];
    snprintf(object, sizeof(object), "u%u_%s", (unsigned)(u + 1), aid);
    if (!upsertControl_(object, ActionButton, u, rawAid, "")) return;
    char aname[32];
    actionName(a, aname, sizeof(aname));
    char name[48];
    if (units_ > 1) snprintf(name, sizeof(name), "U%u %s", (unsigned)(u + 1), aname);
    else            copyStr(name, sizeof(name), aname);
    const char* dc = a["device_class"] | "";
    StaticJsonDocument<1536> cfg;
    cfg["name"] = name;
    char cmd[96];
    topic_(cmd, sizeof(cmd), object, "set");
    cfg["command_topic"] = cmd;
    cfg["payload_press"] = "PRESS";
    if (!strcmp(dc, "identify")) cfg["device_class"] = "identify";
    else if (!strcmp(dc, "clear_errors")) cfg["icon"] = "mdi:alert-remove";
    else if (!strcmp(a["mode"] | "", "IDLE")) cfg["icon"] = "mdi:stop";
    sendConfig_(CompButton, object, cfg);
}

void HaCardProjection::sendConfig_(Comp comp, const char* object, JsonDocument& cfg) {
    char uid[80], avty[80], dev[48];
    snprintf(uid, sizeof(uid), "idryer_%s_%s", serial_, object);
    snprintf(avty, sizeof(avty), "idryer/%s/ha/availability", serial_);
    snprintf(dev, sizeof(dev), "idryer_%s", serial_);
    cfg["unique_id"] = uid;
    cfg["availability_topic"] = avty;
    JsonObject d = cfg.createNestedObject("device");
    d.createNestedArray("identifiers").add(dev);
    d["name"]          = model_;
    d["manufacturer"]  = "iDryer";
    d["model"]         = model_;
    d["serial_number"] = serial_;
    if (fw_[0]) d["sw_version"] = fw_;
    if (hw_[0]) d["hw_version"] = hw_;
    if (cfg.overflowed() || measureJson(cfg) >= sizeof(buf_)) {
        HAL_LOG_ERROR("HA_CARD", "config too large: %s", object);
        return;
    }
    serializeJson(cfg, buf_, sizeof(buf_));
    char topic[96];
    snprintf(topic, sizeof(topic), "homeassistant/%s/idryer_%s/%s/config", kComp[comp], serial_, object);
    mqtt_->publish(topic, buf_, true);

    for (uint8_t i = 0; i < publishedCount_; ++i) {
        if (!strcmp(published_[i].object, object) && published_[i].comp == comp) {
            published_[i].seen = true;
            return;
        }
    }
    if (publishedCount_ >= MAX_PUBLISHED) return;
    Published& p = published_[publishedCount_++];
    copyStr(p.object, sizeof(p.object), object);
    p.comp = comp;
    p.seen = true;
}

// ── Уборка прежнего формата ─────────────────────────────────────────────────
// Прежнее ядро публиковало homeassistant/{comp}/idryer_{id}_{name}/config:
// датчики и управление по юнитам и ручные контролы продуктов. Имена конечны —
// удаляем по списку, без подписки на конфиги всех устройств брокера.

namespace {
struct LegacyName { uint8_t comp; const char* name; };
constexpr LegacyName kLegacyUnit[] = {
    {0, "temperature"}, {0, "humidity"}, {0, "heater_power"}, {0, "mode"}, {0, "weight"},
    {1, "fan"}, {3, "mode_control"}, {2, "set_temp"}, {2, "set_duration"},
};
constexpr LegacyName kLegacyDevice[] = {
    {0, "alerts"},
    {2, "dry_temp"}, {2, "dry_time"}, {2, "heat_temp"}, {2, "heat_duration"},
    {5, "start_drying"}, {5, "start_storage"}, {5, "stop"}, {5, "heat_start"},
    {5, "heat_50"}, {5, "heat_55"}, {5, "heat_60"},
};
constexpr uint8_t kLegacyUnitN   = sizeof(kLegacyUnit) / sizeof(kLegacyUnit[0]);
constexpr uint8_t kLegacyDeviceN = sizeof(kLegacyDevice) / sizeof(kLegacyDevice[0]);
constexpr uint8_t kLegacyPerId   = 4 * kLegacyUnitN + kLegacyDeviceN;
} // namespace

// Одна публикация за вызов; false — список пройден.
bool HaCardProjection::emitLegacy_() {
    const uint8_t ids = legacyId_[0] ? 2 : 1;
    if (legacyStep_ >= ids * kLegacyPerId) { legacyStep_ = 0xFF; return false; }
    const uint8_t s = legacyStep_++;
    const char* id = (s / kLegacyPerId) ? legacyId_ : serial_;
    const uint8_t k = s % kLegacyPerId;
    char topic[112];
    if (k < 4 * kLegacyUnitN) {
        const LegacyName& n = kLegacyUnit[k % kLegacyUnitN];
        snprintf(topic, sizeof(topic), "homeassistant/%s/idryer_%s_U%u_%s/config",
                 kComp[n.comp], id, (unsigned)(k / kLegacyUnitN + 1), n.name);
    } else {
        const LegacyName& n = kLegacyDevice[k - 4 * kLegacyUnitN];
        snprintf(topic, sizeof(topic), "homeassistant/%s/idryer_%s_%s/config", kComp[n.comp], id, n.name);
    }
    mqtt_->publish(topic, "", true);
    return true;
}

// ── Контролы ────────────────────────────────────────────────────────────────

HaCardProjection::Control* HaCardProjection::control_(const char* object) {
    for (uint8_t i = 0; i < controlCount_; ++i) {
        if (!strcmp(controls_[i].object, object)) return &controls_[i];
    }
    return nullptr;
}

HaCardProjection::Control* HaCardProjection::upsertControl_(const char* object, Kind kind,
                                                            uint8_t unit, const char* action,
                                                            const char* param) {
    Control* c = control_(object);
    if (!c) {
        if (controlCount_ >= MAX_CONTROLS) {
            HAL_LOG_WARN("HA_CARD", "too many controls, skip %s", object);
            return nullptr;
        }
        c = &controls_[controlCount_++];
        memset(c, 0, sizeof(*c));
        copyStr(c->object, sizeof(c->object), object);
        c->step = 1;
    }
    c->kind = kind;
    c->unit = unit;
    copyStr(c->action, sizeof(c->action), action);
    copyStr(c->param, sizeof(c->param), param);
    c->live = true;
    return c;
}

void HaCardProjection::publishState_(const Control& c) {
    char topic[96];
    topic_(topic, sizeof(topic), c.object, "state");
    if (c.kind == ParamNumber) {
        const int dec = c.step >= 1 ? 0 : (c.step >= 0.1f ? 1 : 2);
        char v[24];
        snprintf(v, sizeof(v), "%.*f", dec, (double)c.num);
        mqtt_->publish(topic, v, true);
    } else {
        mqtt_->publish(topic, c.str, true);
    }
}

void HaCardProjection::execute_(const Control& c) {
    if (!invokeFn_) return;
    StaticJsonDocument<512> cmd;
    char action[32], unit[4];
    snprintf(action, sizeof(action), "card.%s", c.action);
    snprintf(unit, sizeof(unit), "U%u", (unsigned)(c.unit + 1));
    cmd["action"] = action;
    cmd["unitId"] = unit;
    JsonObject args = cmd.createNestedObject("args");
    if (c.kind == ActionButton) {
        // Параметры действия — текущие значения полей этого юнита.
        for (uint8_t i = 0; i < controlCount_; ++i) {
            const Control& p = controls_[i];
            if (p.unit != c.unit || strcmp(p.action, c.action) != 0) continue;
            if (p.kind == ParamNumber) args[p.param] = p.num;
            else if (p.kind == ParamSelect || p.kind == ParamColor) args[p.param] = p.str;
        }
    } else if (c.kind == EntityNumber) {
        args["value"] = c.num;
    } else if (c.kind == EntitySelect) {
        args["value"] = c.str;
    }
    HAL_LOG_INFO("HA_CARD", "invoke %s %s", action, unit);
    invokeFn_(invokeCtx_, cmd.as<JsonObjectConst>());
}

// ── Входящие ────────────────────────────────────────────────────────────────

void HaCardProjection::handleIncoming(const char* topic, const char* payload) {
    if (!topic || !serial_[0]) return;
    if (!payload) payload = "";

    char prefix[64];
    snprintf(prefix, sizeof(prefix), "idryer/%s/ha/", serial_);
    const size_t pl = strlen(prefix);
    if (!strncmp(topic, prefix, pl)) {
        const char* obj = topic + pl;
        const char* slash = strchr(obj, '/');
        if (!slash || strcmp(slash, "/set") != 0) return;
        const size_t n = (size_t)(slash - obj);
        if (n == 0 || n >= sizeof(Control::object)) return;
        char object[sizeof(Control::object)];
        memcpy(object, obj, n);
        object[n] = '\0';
        Control* c = control_(object);
        if (!c) return;
        bool queue = false;
        switch (c->kind) {
            case ParamNumber:
            case EntityNumber: {
                float v = (float)atof(payload);
                if (c->kind == ParamNumber) {
                    if (v < c->min) v = c->min;
                    if (v > c->max) v = c->max;
                }
                c->num = v;
                c->stateDirty = c->kind == ParamNumber;
                queue = c->kind == EntityNumber;
                break;
            }
            case ParamSelect:
            case EntitySelect:
                copyStr(c->str, sizeof(c->str), payload);
                c->stateDirty = c->kind == ParamSelect;
                queue = c->kind == EntitySelect;
                break;
            case ParamColor:
                if (!isHexColor(payload)) return;
                copyStr(c->str, sizeof(c->str), payload);
                c->stateDirty = true;
                break;
            case ActionButton:
            case EntityButton:
                queue = true;
                break;
        }
        if (queue && pendingCount_ < MAX_PENDING) pending_[pendingCount_++] = (uint8_t)(c - controls_);
        return;
    }

    // Окно уборки: свои retained-конфиги homeassistant/{comp}/idryer_{serial}/{object}/config.
    // Решение — в конце окна: к нему прибор успеет опубликовать полный манифест.
    if (!cleanupUntilMs_ || !payload[0] || strncmp(topic, "homeassistant/", 14) != 0) return;
    const char* comp = topic + 14;
    const char* s1 = strchr(comp, '/');
    if (!s1) return;
    char node[48];
    snprintf(node, sizeof(node), "idryer_%s/", serial_);
    if (strncmp(s1 + 1, node, strlen(node)) != 0) return;
    const char* obj = s1 + 1 + strlen(node);
    const char* s3 = strchr(obj, '/');
    if (!s3 || strcmp(s3, "/config") != 0) return;
    const size_t n = (size_t)(s3 - obj);
    if (n == 0 || n >= sizeof(Published::object)) return;
    char object[sizeof(Published::object)];
    memcpy(object, obj, n);
    object[n] = '\0';
    if (isPublished_(object)) return;
    for (uint8_t c = 0; c < sizeof(kComp) / sizeof(kComp[0]); ++c) {
        const size_t cl = strlen(kComp[c]);
        if ((size_t)(s1 - comp) == cl && !strncmp(comp, kComp[c], cl)) {
            queueStale_(object, (Comp)c);
            return;
        }
    }
}

bool HaCardProjection::isPublished_(const char* object) const {
    for (uint8_t i = 0; i < publishedCount_; ++i) {
        if (!strcmp(published_[i].object, object)) return true;
    }
    return false;
}

void HaCardProjection::queueStale_(const char* object, Comp comp) {
    for (uint8_t i = 0; i < staleCount_; ++i) {
        if (stale_[i].comp == comp && !strcmp(stale_[i].object, object)) return;
    }
    if (staleCount_ >= MAX_STALE) return;
    copyStr(stale_[staleCount_].object, sizeof(stale_[0].object), object);
    stale_[staleCount_].comp = comp;
    ++staleCount_;
}

void HaCardProjection::mirror(const char* kind, JsonDocument& doc) {
    if (!connected_ || !subscribed_ || !mqtt_ || !mqtt_->isConnected() || !serial_[0]) return;
    if (measureJson(doc) >= sizeof(buf_)) return;
    serializeJson(doc, buf_, sizeof(buf_));
    char topic[64];
    snprintf(topic, sizeof(topic), "idryer/%s/%s", serial_, kind);
    mqtt_->publish(topic, buf_, true);
}

// ── Цикл ────────────────────────────────────────────────────────────────────

void HaCardProjection::loop() {
    if (!connected_ || !mqtt_ || !mqtt_->isConnected()) return;

    // Команды — сразу: их мало, и человек ждёт отклика.
    const uint8_t n = pendingCount_;
    pendingCount_ = 0;
    for (uint8_t i = 0; i < n; ++i) {
        if (pending_[i] < controlCount_) execute_(controls_[pending_[i]]);
    }

    if (phase_ == PhIdle && needPublish_ && (int32_t)(millis() - publishAfterMs_) >= 0) startPass_();

    if (cleanupUntilMs_ && (int32_t)(millis() - cleanupUntilMs_) >= 0) {
        cleanupUntilMs_ = 0;
        char filter[96];
        snprintf(filter, sizeof(filter), "homeassistant/+/idryer_%s/+/config", serial_);
        mqtt_->unsubscribe(filter);
    }

    // Остальное — порциями.
    uint8_t budget = PER_LOOP;
    while (budget && phase_ != PhIdle) {
        if (!emitNext_()) { finishPass_(); break; }
        --budget;
    }
    for (uint8_t i = 0; budget && i < controlCount_; ++i) {
        if (!controls_[i].stateDirty) continue;
        controls_[i].stateDirty = false;
        publishState_(controls_[i]);
        --budget;
    }
    while (budget && phase_ == PhIdle && legacyStep_ != 0xFF) {
        if (!emitLegacy_()) break;
        --budget;
    }
    // Удаление — после окна уборки и только того, что так и не опубликовано.
    while (budget && !cleanupUntilMs_ && phase_ == PhIdle && staleCount_ > 0) {
        const Published& s = stale_[--staleCount_];
        if (isPublished_(s.object)) continue;
        char topic[96];
        snprintf(topic, sizeof(topic), "homeassistant/%s/idryer_%s/%s/config", kComp[s.comp], serial_, s.object);
        mqtt_->publish(topic, "", true);
        HAL_LOG_INFO("HA_CARD", "removed stale %s", topic);
        --budget;
    }
}

} // namespace ha
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
