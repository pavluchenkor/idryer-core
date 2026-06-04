// MenuPublisher — pre-allocated публикатор полного config меню в MQTT.
//
// Решает три проблемы предыдущего подхода (menu_buildFullJson + продуктовый
// malloc):
//   1. static StaticJsonDocument<MENU_JSON_DOC_CAP> в .bss — на DRYER (idryer-
//      link, 202 пункта) занимает ~22КБ постоянно, блокирует TLS-handshake
//      mbedtls на ESP32-C3 (heap fragmentation).
//   2. malloc(MENU_FULL_JSON_BUF_SIZE) на каждый publishConfig — фрагментирует
//      heap, после нескольких MQTT-reconnect'ов аллокация может упасть.
//   3. MENU_FULL_JSON_BUF_SIZE = capacity in-memory структуры ArduinoJson
//      использовалось как буфер под сериализованный текст — концептуальная
//      путаница (capacity ≠ serialized size).
//
// Решение: один malloc на старте после s_link.begin() (TLS уже прошёл, heap
// максимально свободен), переиспользуется всю жизнь устройства. Никакой .bss-
// памяти под буфер.
//
// Использование:
//
//   #include "menu_publisher.h"
//
//   static idryer::MenuPublisher s_menuPub;
//
//   void setup() {
//       ...
//       s_link.begin();
//       if (!s_menuPub.begin()) {
//           HAL_LOG_ERROR("MENU", "MenuPublisher init failed at boot");
//           // продукт сам решает: рестарт / retry / работа без публикации.
//       }
//   }
//
//   // На событие "получен новый config от MCU":
//   size_t sent = s_menuPub.publishFull(s_link.devicePublisher());
//   if (sent == 0) {
//       publishMenuError("PUBLISH_FAILED", "menu publishFull returned 0");
//   }

#pragma once

#include <ArduinoJson.h>
#include <stdlib.h>
#include "menu_meta.h"     // MENU_META_COUNT, MENU_SERIALIZED_MAX_SIZE, g_menu_meta
#include "menu_cache.h"    // g_menu_cache, MENU_MAX_UNITS
#include "menu_commands.h" // MENU_JSON_DOC_CAP (capacity для DynamicJsonDocument)

// Backward-compat fallback: MENU_SERIALIZED_MAX_SIZE генерируется menu_gen.py
// (точный размер сериализованного текста по содержимому меню). Если генератор
// ещё не перезапущен для продукта — используем старую константу из
// menu_commands.h как safe upper bound. Это завышено (использует capacity
// in-memory структуры вместо реального размера текста), но не сломает сборку.
#ifndef MENU_SERIALIZED_MAX_SIZE
#define MENU_SERIALIZED_MAX_SIZE MENU_FULL_JSON_BUF_SIZE
#endif

namespace idryer {

class MenuPublisher {
public:
    /// Pre-allocate heap-буфер под сериализованный JSON + JsonDocument.
    /// Вызывать ОДИН РАЗ после s_link.begin() — гарантия что TLS-handshake
    /// уже прошёл и contiguous heap максимально свободен.
    /// @return true если оба malloc'а удались. Иначе false — продукт решает
    ///         что делать (рестарт / retry / работа без публикации меню).
    bool begin() {
        if (textBuf_ && doc_) return true; // уже инициализирован

        if (!textBuf_) {
            textBuf_ = (char*)malloc(MENU_SERIALIZED_MAX_SIZE);
            if (!textBuf_) return false;
        }
        if (!doc_) {
            // На ESP-IDF дефолт компилируется с -fno-exceptions, поэтому new
            // возвращает nullptr при OOM (а не бросает bad_alloc).
            doc_ = new DynamicJsonDocument(MENU_JSON_DOC_CAP);
            if (!doc_) {
                free(textBuf_);
                textBuf_ = nullptr;
                return false;
            }
        }
        return true;
    }

    /// Собрать актуальный snapshot g_menu_meta + g_menu_cache и опубликовать
    /// через publisher->publishConfigRaw(buf, len). Использует pre-allocated
    /// ресурсы — никаких malloc/free в горячем пути.
    ///
    /// publisher — указатель на объект с методом
    ///   publishConfigRaw(const char* data, size_t len)
    /// (Шаблон, т.к. DevicePublisher живёт в Link и явная зависимость отсюда
    /// нежелательна — избегаем циклов include).
    ///
    /// @return длина сериализованного JSON в байтах или 0 при ошибке (begin()
    ///         не был вызван / overflow / serialize fail).
    template <typename PublisherT>
    size_t publishFull(PublisherT* publisher) {
        if (!textBuf_ || !doc_ || !publisher) return 0;

        doc_->clear();

        // Та же логика что в menu_buildFullJson — копирована один-в-один,
        // чтобы JSON-формат для backend не менялся.
        (*doc_)["v"] = g_menu_cache.revision;

        if (MENU_META_COUNT >= 2) {
            uint16_t unitsCountId = MENU_META_COUNT - 2;
            uint8_t unitsCountValue = (uint8_t)g_menu_cache.getInt(unitsCountId, 0);
            if (unitsCountValue > 0 && unitsCountValue <= MENU_MAX_UNITS) {
                g_menu_cache.units_count = unitsCountValue;
            }
            uint16_t langId = MENU_META_COUNT - 1;
            uint8_t langValue = (uint8_t)g_menu_cache.getInt(langId, 0);
            g_menu_cache.lang = langValue;
        }

        JsonArray menu = doc_->createNestedArray("menu");
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

        if (doc_->overflowed()) {
            return 0; // не публикуем усечённый JSON
        }

        size_t len = serializeJson(*doc_, textBuf_, MENU_SERIALIZED_MAX_SIZE);
        if (len == 0) return 0;

        publisher->publishConfigRaw(textBuf_, len);
        return len;
    }

    ~MenuPublisher() {
        if (textBuf_) {
            free(textBuf_);
            textBuf_ = nullptr;
        }
        if (doc_) {
            delete doc_;
            doc_ = nullptr;
        }
    }

    // Запретить копирование — владение malloc/new строго единичное.
    MenuPublisher() = default;
    MenuPublisher(const MenuPublisher&) = delete;
    MenuPublisher& operator=(const MenuPublisher&) = delete;

private:
    char* textBuf_ = nullptr;
    DynamicJsonDocument* doc_ = nullptr;
};

} // namespace idryer
