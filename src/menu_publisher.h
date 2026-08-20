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
#include "hal/hal_types.h" // HAL_LOG_* — заголовок используется и там,
                           // где HAL ещё не был подключён продуктом
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
    /// Ничего не выделяет: меню собирается и уходит потоком, буферы живут
    /// только на время публикации. Метод оставлен ради совместимости с
    /// продуктами, которые звали его в setup().
    bool begin() { return true; }

    /// Собрать актуальный snapshot g_menu_meta + g_menu_cache и опубликовать.
    ///
    /// Меню целиком (у DRYER это ~26 КБ) в память не помещается: на платах с
    /// дисплеем свободной кучи единицы килобайт, а держать её заранее нельзя —
    /// тогда не хватает сетевому стеку и не поднимается MQTT. Поэтому JSON
    /// генерируется по одному пункту (каждый — крошечный документ на стеке,
    /// формат остаётся байт в байт прежним) и уходит кусками, как только
    /// накопится MENU_CHUNK_SIZE.
    ///
    /// publisher — объект с методами publishConfigRaw(data, len) для короткого
    /// меню и publishRawChunkTopic(...)/publishConfigRaw для чанков.
    ///
    /// @return длина сериализованного JSON или 0 при ошибке.
    template <typename PublisherT>
    size_t publishFull(PublisherT* publisher) {
        if (!publisher) return 0;

        // Первый проход — только считаем длину: она нужна в заголовке каждого
        // куска (поле total), а портал по ней понимает, сколько ещё ждать.
        CountingSink counter;
        if (!generate(counter)) return 0;
        const size_t total = counter.total;
        if (total == 0) return 0;

        ChunkSinkT<PublisherT> sink(publisher, total);
        if (!sink.ready()) {
            HAL_LOG_ERROR("MENU", "no memory for chunk buffers");
            return 0;
        }
        if (!generate(sink) || !sink.finish()) {
            HAL_LOG_ERROR("MENU", "publish failed at chunk %u", (unsigned)sink.chunks());
            return 0;
        }
        HAL_LOG_INFO("MENU", "config published: %u bytes in %u chunks",
                     (unsigned)total, (unsigned)sink.chunks());
        return total;
    }

    MenuPublisher() = default;
    MenuPublisher(const MenuPublisher&) = delete;
    MenuPublisher& operator=(const MenuPublisher&) = delete;

private:
    /// Сколько сырого JSON накапливаем перед отправкой куска. Маленький кусок
    /// намеренно: и он сам, и конверт вокруг него лежат на стеке, поэтому
    /// публикация не зависит от того, насколько раздроблена куча. Раньше здесь
    /// были malloc'и, и на фрагментированной куче отправка падала на середине
    /// («publish failed at chunk 7»).
    static constexpr size_t MENU_CHUNK_SIZE = 512;
    /// Худший случай экранирования — каждый байт превращается в два, плюс
    /// заголовок конверта.
    static constexpr size_t MENU_ENV_CAP = MENU_CHUNK_SIZE * 2 + 96;

    /// Приёмник, который только считает байты (первый проход).
    struct CountingSink {
        size_t total = 0;
        bool put(const char* /*data*/, size_t len) { total += len; return true; }
    };

    /// Приёмник, который копит байты и публикует их кусками в формате
    /// {tid, idx, total, last, d} — том же, что понимает портал.
    template <typename PublisherT>
    struct ChunkSinkT {
        ChunkSinkT(PublisherT* pub, size_t total) : pub_(pub), total_(total) {
            static uint16_t s_tid = 0;
            tid_ = ++s_tid;
        }

        bool ready() const { return true; }   // ничего не выделяем
        uint16_t chunks() const { return idx_; }

        bool put(const char* data, size_t len) {
            size_t done = 0;
            while (done < len) {
                const size_t room = MENU_CHUNK_SIZE - fill_;
                const size_t take = (len - done < room) ? (len - done) : room;
                memcpy(raw_ + fill_, data + done, take);
                fill_ += take;
                done  += take;
                sent_ += take;
                if (fill_ == MENU_CHUNK_SIZE && !flush(sent_ >= total_)) return false;
            }
            return true;
        }

        bool finish() { return fill_ == 0 ? true : flush(true); }

    private:
        /// Конверт собираем вручную: ArduinoJson здесь требовал бы документ на
        /// пару килобайт из кучи, а нам нужна отправка, не зависящая от неё.
        bool flush(bool last) {
            char env[MENU_ENV_CAP];
            int n = snprintf(env, sizeof(env),
                             "{\"tid\":%u,\"idx\":%u,\"total\":%u,\"last\":%s,\"d\":\"",
                             (unsigned)tid_, (unsigned)idx_, (unsigned)total_,
                             last ? "true" : "false");
            if (n <= 0) return false;
            size_t pos = (size_t)n;

            // Экранирование строки JSON: кавычка, обратный слэш и управляющие
            // символы. Русские подписи — обычный UTF-8, идут как есть.
            for (size_t i = 0; i < fill_; i++) {
                const unsigned char c = (unsigned char)raw_[i];
                if (pos + 8 >= sizeof(env)) return false;
                if (c == '"' || c == '\\') {
                    env[pos++] = '\\';
                    env[pos++] = (char)c;
                } else if (c < 0x20) {
                    pos += (size_t)snprintf(env + pos, sizeof(env) - pos, "\\u%04x", c);
                } else {
                    env[pos++] = (char)c;
                }
            }
            if (pos + 3 >= sizeof(env)) return false;
            env[pos++] = '"';
            env[pos++] = '}';
            env[pos]   = '\0';

            // Первый кусок заодно стирает устаревший retained-снимок меню.
            if (pub_->publishConfigChunk(env, pos, idx_ == 0) == 0) return false;
            idx_++;
            fill_ = 0;
            return true;
        }

        PublisherT* pub_;
        size_t   total_;
        char     raw_[MENU_CHUNK_SIZE];
        size_t   fill_ = 0;
        size_t   sent_ = 0;
        uint16_t idx_  = 0;
        uint16_t tid_  = 0;
    };

    /// Единственное место, где описан формат меню. Оба прохода (подсчёт и
    /// публикация) идут через него, поэтому длина и содержимое не разъезжаются.
    template <typename SinkT>
    static bool generate(SinkT& sink) {
        char head[48];
        int n = snprintf(head, sizeof(head), "{\"v\":%u,\"menu\":[",
                         (unsigned)g_menu_cache.revision);
        if (n <= 0 || !sink.put(head, (size_t)n)) return false;

        if (MENU_META_COUNT >= 2) {
            uint16_t unitsCountId = MENU_META_COUNT - 2;
            uint8_t unitsCountValue = (uint8_t)g_menu_cache.getInt(unitsCountId, 0);
            if (unitsCountValue > 0 && unitsCountValue <= MENU_MAX_UNITS) {
                g_menu_cache.units_count = unitsCountValue;
            }
            uint16_t langId = MENU_META_COUNT - 1;
            g_menu_cache.lang = (uint8_t)g_menu_cache.getInt(langId, 0);
        }

        const uint8_t lang = g_menu_cache.getLang();
        uint8_t unitsCount = g_menu_cache.getUnitsCount();
        if (unitsCount == 0) unitsCount = 1;

        for (uint16_t id = 0; id < MENU_META_COUNT; id++) {
            const MenuMeta* meta = &g_menu_meta[id];

            // Пункт целиком — на стеке. Формат тот же, что и раньше: ту же
            // сериализацию делает та же библиотека, просто по одному объекту.
            StaticJsonDocument<512> item;
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
            if (meta->unit[lang]) item["u"] = meta->unit[lang];
            if (meta->role)       item["r"] = meta->role;

            if (meta->type == META_VALUE || meta->type == META_TOGGLE) {
                if (meta->type == META_VALUE) {
                    item["min"]  = meta->min_val;
                    item["max"]  = meta->max_val;
                    item["step"] = meta->step;
                }
                if (meta->scope == META_SCOPE_GLOBAL) {
                    if (meta->type == META_TOGGLE) item["val"] = g_menu_cache.getBool(id, 0);
                    else                           item["val"] = g_menu_cache.getFloat(id, 0);
                } else {
                    JsonArray vals = item.createNestedArray("val");
                    for (uint8_t u = 0; u < unitsCount; u++) {
                        if (meta->type == META_TOGGLE) vals.add(g_menu_cache.getBool(id, u));
                        else                           vals.add(g_menu_cache.getFloat(id, u));
                    }
                }
            }
            if (item.overflowed()) {
                HAL_LOG_ERROR("MENU", "item %u does not fit 512 bytes", (unsigned)id);
                return false;
            }

            char buf[512];
            const size_t len = serializeJson(item, buf, sizeof(buf));
            if (len == 0) return false;
            if (id != 0 && !sink.put(",", 1)) return false;
            if (!sink.put(buf, len)) return false;
        }

        return sink.put("]}", 2);
    }
};

} // namespace idryer
