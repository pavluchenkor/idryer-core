/**
 * @file menu_commands.h
 * @brief Парсинг JSON конфигов и формирование команд меню (header-only).
 *
 * Generic runtime-helper для клиентской стороны меню (ESP32): парсит config/delta
 * JSON в g_menu_cache и собирает set/invoke/get_config команды + полный config JSON
 * для портала. Транспорт (UART/MQTT) не знает.
 *
 * Header-only намеренно: модуль зависит от product-specific menu_meta.h / menu_cache.h
 * (генерируются в каждом продукте из своего menu.yaml). Как header он компилируется
 * в контексте включающего .cpp, где эти заголовки доступны — поэтому одна общая
 * копия в idryer-core работает для всех продуктов без копирования.
 *
 * Состояние (callback, парсинг-буфер) обёрнуто в inline-accessor'ы (Meyers-singleton),
 * чтобы при включении в несколько TU оставалась одна копия на программу.
 *
 * @example
 *   menu_parseFullConfig(jsonFromMcu);
 *   float temp = g_menu_cache.getFloat(3); // dry_temp
 *   char buf[64];
 *   menu_buildSetCommand(buf, sizeof(buf), 3, 0, 55.0f);
 *   // buf = {"cmd":"set","id":3,"unit":0,"val":55}
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ArduinoJson.h>
#include "menu_meta.h"
#include "menu_cache.h"

// Расчёт вместимости JSON для полного меню
constexpr size_t MENU_JSON_ITEM_CAP =
    JSON_OBJECT_SIZE(8) + JSON_ARRAY_SIZE(MENU_MAX_UNITS); // id,t,n,p,u,min,max,step,val[]
constexpr size_t MENU_JSON_DOC_CAP =
    JSON_OBJECT_SIZE(2) +                             // v + menu
    JSON_ARRAY_SIZE(MENU_META_COUNT) +                // оболочка массива
    MENU_META_COUNT * MENU_JSON_ITEM_CAP;             // сами элементы
constexpr size_t MENU_FULL_JSON_BUF_SIZE = MENU_JSON_DOC_CAP + 256; // запас под сериализацию

// ============================================================================
// Callback при изменении значения в кэше
// ============================================================================

/// Вызывается после успешного парсинга delta/full config.
/// @param id   ID изменённого элемента
/// @param unit Номер юнита (255 для global)
typedef void (*MenuChangeCallback)(uint16_t id, uint8_t unit);

namespace idryer_menu_detail {

// Meyers-singleton'ы: одна копия состояния на программу даже при включении
// header в несколько TU.
inline MenuChangeCallback& changeCallback() {
    static MenuChangeCallback cb = nullptr;
    return cb;
}

inline StaticJsonDocument<8192>& configDoc() {
    static StaticJsonDocument<8192> doc;
    return doc;
}

inline void notifyChange(uint16_t id, uint8_t unit) {
    MenuChangeCallback cb = changeCallback();
    if (cb) cb(id, unit);
}

// Конвертация поля lang ("ru"/"en" или число) в индекс языка.
inline uint8_t parseLangVariant(JsonVariant langVal) {
    if (langVal.is<const char*>()) {
        const char* s = langVal.as<const char*>();
        if (!s) return 0;
        if (strcmp(s, "en") == 0 || strcmp(s, "EN") == 0) return 1;
        if (strcmp(s, "1") == 0) return 1;
        return 0; // по умолчанию ru
    }
    uint8_t langIdx = langVal.as<uint8_t>();
    return langIdx ? 1 : 0;
}

inline void applyLangToCache(uint8_t lang) {
    uint8_t norm = lang ? 1 : 0;
    g_menu_cache.lang = norm;
    // Синхронизируем с пунктом LANGUAGE (последний элемент)
    const uint16_t langId = MENU_META_COUNT - 1;
    if (langId < MENU_META_COUNT) {
        g_menu_cache.setFloat(langId, norm, 0);
    }
}

inline void applyUnitsToCache(uint8_t units) {
    uint8_t norm = (units >= 1 && units <= MENU_MAX_UNITS) ? units : g_menu_cache.units_count;
    g_menu_cache.units_count = norm;
    // Синхронизируем с пунктом UNITS_COUNT (предпоследний элемент)
    const uint16_t unitsId = MENU_META_COUNT - 2;
    if (unitsId < MENU_META_COUNT) {
        g_menu_cache.setFloat(unitsId, norm, 0);
    }
}

/// Парсинг vals объекта (общая логика для full и delta).
/// @param vals JsonObject: ключи = ID, значения = val или [val, val, val]
inline void parseValsObject(JsonObject vals) {
    for (JsonPair kv : vals) {
        uint16_t id = (uint16_t)atoi(kv.key().c_str());
        if (id >= MENU_META_COUNT) continue;

        const MenuMeta* meta = menu_meta_get(id);
        if (!meta) continue;

        JsonVariant val = kv.value();

        if (meta->scope == META_SCOPE_GLOBAL) {
            g_menu_cache.setFloat(id, val.as<float>(), 0);
            notifyChange(id, 255);
        } else {
            if (val.is<JsonArray>()) {
                JsonArray arr = val.as<JsonArray>();
                uint8_t u = 0;
                for (JsonVariant v : arr) {
                    if (u >= MENU_MAX_UNITS) break;
                    g_menu_cache.setFloat(id, v.as<float>(), u);
                    notifyChange(id, u);
                    u++;
                }
            } else {
                g_menu_cache.setFloat(id, val.as<float>(), 0);
                notifyChange(id, 0);
            }
        }

        // Мгновенное обновление языка/юнитов при их приходе в vals
        const uint16_t langId = MENU_META_COUNT - 1;
        const uint16_t unitsId = MENU_META_COUNT - 2;
        if (id == langId) {
            applyLangToCache((uint8_t)val.as<uint8_t>());
        } else if (id == unitsId) {
            applyUnitsToCache((uint8_t)val.as<uint8_t>());
        }
    }
}

} // namespace idryer_menu_detail

// ============================================================================
// Парсинг JSON от MCU → обновление g_menu_cache
// ============================================================================

/// Парсинг полного конфига от MCU. Формат: {"rev":8,"units":3,"active":0,
/// "lang":"ru","vals":{"3":[55,55,60],"143":3}}. @return true при успехе.
inline bool menu_parseFullConfig(const char* json) {
    if (!json) return false;

    auto& doc = idryer_menu_detail::configDoc();
    doc.clear();
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    // Revision (rev или v)
    if (doc.containsKey("rev")) {
        g_menu_cache.revision = doc["rev"].as<uint16_t>();
    } else if (doc.containsKey("v")) {
        g_menu_cache.revision = doc["v"].as<uint16_t>();
    }

    // Старый формат: units/active/lang в корне
    if (doc.containsKey("units")) {
        idryer_menu_detail::applyUnitsToCache(doc["units"].as<uint8_t>());
    }
    if (doc.containsKey("active")) {
        uint8_t active = doc["active"].as<uint8_t>();
        if (active < MENU_MAX_UNITS) {
            g_menu_cache.active_unit = active;
        }
    }
    if (doc.containsKey("lang")) {
        idryer_menu_detail::applyLangToCache(idryer_menu_detail::parseLangVariant(doc["lang"]));
    }

    JsonObject vals = doc["vals"].as<JsonObject>();
    if (!vals) return false;

    idryer_menu_detail::parseValsObject(vals);
    return true;
}

/// Парсинг delta-обновления. Формат: {"rev":124,"vals":{"3":[55,55,60]}}.
/// @return true при успехе.
inline bool menu_parseDelta(const char* json) {
    if (!json) return false;

    auto& doc = idryer_menu_detail::configDoc();
    doc.clear();
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    if (doc.containsKey("rev")) {
        g_menu_cache.revision = doc["rev"].as<uint16_t>();
    }
    if (doc.containsKey("lang")) {
        idryer_menu_detail::applyLangToCache(idryer_menu_detail::parseLangVariant(doc["lang"]));
    }

    JsonObject vals = doc["vals"].as<JsonObject>();
    if (!vals) return false;

    idryer_menu_detail::parseValsObject(vals);
    return true;
}

// ============================================================================
// Формирование JSON команд → MCU
// ============================================================================

/// {"cmd":"set","id":3,"unit":0,"val":55.5}. unit=255 → active_unit.
/// Для global unit игнорируется. @return длина JSON или 0 при ошибке.
inline size_t menu_buildSetCommand(char* buf, size_t bufSize,
                                   uint16_t id, uint8_t unit, float val) {
    if (!buf || bufSize < 32) return 0;

    const MenuMeta* meta = menu_meta_get(id);
    if (!meta) return 0;

    if (meta->scope == META_SCOPE_GLOBAL) {
        return snprintf(buf, bufSize,
            "{\"cmd\":\"set\",\"id\":%u,\"val\":%.2f}",
            id, val);
    }
    if (unit == 255) {
        unit = g_menu_cache.active_unit;
    }
    return snprintf(buf, bufSize,
        "{\"cmd\":\"set\",\"id\":%u,\"unit\":%u,\"val\":%.2f}",
        id, unit, val);
}

/// {"cmd":"invoke","id":89}. @return длина JSON или 0 при ошибке.
inline size_t menu_buildInvokeCommand(char* buf, size_t bufSize, uint16_t id) {
    if (!buf || bufSize < 24) return 0;
    return snprintf(buf, bufSize, "{\"cmd\":\"invoke\",\"id\":%u}", id);
}

/// {"cmd":"get_config"}. @return длина JSON или 0 при ошибке.
inline size_t menu_buildGetConfigCommand(char* buf, size_t bufSize) {
    if (!buf || bufSize < 20) return 0;
    return snprintf(buf, bufSize, "{\"cmd\":\"get_config\"}");
}

/// {"cmd":"set_active","unit":1}. @return длина JSON или 0 при ошибке.
inline size_t menu_buildSetActiveCommand(char* buf, size_t bufSize, uint8_t unit) {
    if (!buf || bufSize < 28) return 0;
    return snprintf(buf, bufSize, "{\"cmd\":\"set_active\",\"unit\":%u}", unit);
}

// ============================================================================
// Callback
// ============================================================================

/// Установить callback на изменение значений (nullptr для отключения).
inline void menu_setChangeCallback(MenuChangeCallback cb) {
    idryer_menu_detail::changeCallback() = cb;
}

// ============================================================================
// Полный JSON меню для бэкенда (menu-as-protocol)
// ============================================================================

/// Объединяет g_menu_meta (структура, названия) + g_menu_cache (значения) в
/// JSON для MQTT. Рекомендуемый буфер 16KB+. @return длина JSON или 0 при ошибке.
inline size_t menu_buildFullJson(char* buf, size_t bufSize) {
    if (!buf || bufSize < 256) return 0;

    static StaticJsonDocument<MENU_JSON_DOC_CAP> doc;
    doc.clear();

    // units/lang теперь обычные пункты меню (предпоследний/последний)
    doc["v"] = g_menu_cache.revision;

    if (MENU_META_COUNT >= 2) {
        uint16_t unitsCountId = MENU_META_COUNT - 2;
        uint8_t unitsCountValue = (uint8_t)g_menu_cache.getInt(unitsCountId, 0);
        if (unitsCountValue > 0 && unitsCountValue <= MENU_MAX_UNITS) {
            g_menu_cache.units_count = unitsCountValue;
        }
        uint16_t langId = MENU_META_COUNT - 1;
        uint8_t langValue = (uint8_t)g_menu_cache.getInt(langId, 0);
        g_menu_cache.lang = langValue; // 0=ru, 1=en
    }

    JsonArray menu = doc.createNestedArray("menu");
    uint8_t lang = g_menu_cache.getLang();
    uint8_t unitsCount = g_menu_cache.getUnitsCount();
    if (unitsCount == 0) unitsCount = 1;

    for (uint16_t id = 0; id < MENU_META_COUNT; id++) {
        const MenuMeta* meta = &g_menu_meta[id];

        JsonObject item = menu.createNestedObject();
        item["id"] = id;

        const char* typeStr = "sub";
        switch (meta->type) {
            case META_SUBMENU: typeStr = "sub"; break;
            case META_ACTION:  typeStr = "act"; break;
            case META_VALUE:   typeStr = "val"; break;
            case META_TOGGLE:  typeStr = "tog"; break;
        }
        item["t"] = typeStr;

        item["n"] = meta->title[lang] ? meta->title[lang] : "";
        item["p"] = meta->parent;

        if (meta->unit[lang]) {
            item["u"] = meta->unit[lang];
        }

        // Каноническая роль для портала (semantic role). Поле widget намеренно
        // НЕ публикуется: виджет = product-specific карточка дашборда, не часть
        // меню (см. ___capabilities_and_menu_as_protocol.md).
        if (meta->role) {
            item["r"] = meta->role;
        }

        if (meta->type == META_VALUE || meta->type == META_TOGGLE) {
            if (meta->type == META_VALUE) {
                item["min"] = meta->min_val;
                item["max"] = meta->max_val;
                item["step"] = meta->step;
            }

            if (meta->scope == META_SCOPE_GLOBAL) {
                if (meta->type == META_TOGGLE) {
                    item["val"] = g_menu_cache.getBool(id, 0);
                } else {
                    item["val"] = g_menu_cache.getFloat(id, 0);
                }
            } else {
                JsonArray vals = item.createNestedArray("val");
                for (uint8_t u = 0; u < unitsCount; u++) {
                    if (meta->type == META_TOGGLE) {
                        vals.add(g_menu_cache.getBool(id, u));
                    } else {
                        vals.add(g_menu_cache.getFloat(id, u));
                    }
                }
            }
        }
    }

    if (doc.overflowed()) {
        return 0; // не рискуем публиковать усечённый JSON
    }

    return serializeJson(doc, buf, bufSize);
}
