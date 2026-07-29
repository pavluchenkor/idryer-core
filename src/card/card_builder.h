/**
 * @file card_builder.h
 * @brief Entity manifest карточки устройства (топик `card`, retained).
 *
 * Самоописание «что показать и чем управлять» для авто-карточки портала
 * и мобильного приложения (слой 1) + опциональная заводская разметка
 * (слой 2). Схема — mqtt_contract.yaml → mqtt_only[suffix=card].
 *
 * Сенсоры из capability_vocabulary генерируются автоматически из
 * Config.has* (продукту делать ничего не надо). Кастомные сенсоры и
 * контролы продукт объявляет через Link::card():
 *
 *   link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
 *   link.card().button("calibrate", "Calibrate", [](){ runCal(); });
 *
 * Значение кастомного сенсора продукт кладёт в telemetry через
 * Link::onTelemetryPublish; `path` — JSON-путь к нему в payload.
 * Контролы прилетают обычным commands/invoke {action:"card.{id}",
 * args:{value}} — либа сама зовёт зарегистрированный колбэк.
 *
 * Raw fnptr (не std::function) — как в HaBuilder. Только stateless лямбды.
 */

#pragma once

#include <ArduinoJson.h>
#include <stdint.h>

#include "../_generated/iDryer_api.h"  // iDryer::Config

namespace idryer {

class CardBuilder {
public:
    using OnPress  = void (*)();
    using OnNumber = void (*)(float value);
    using OnSelect = void (*)(const char* option);

    /// Кастомный сенсор. @p path — JSON-путь значения в telemetry payload
    /// (например "units[0].co2ppm"); @p deviceClass опционален (иконка/формат).
    bool sensor(const char* id, const char* label, const char* unit,
                const char* path, const char* deviceClass = nullptr);

    /// Двоичный сенсор (true/false в telemetry по @p path).
    bool binarySensor(const char* id, const char* label, const char* path);

    /// Кнопка. Нажатие на портале → invoke card.{id} → @p cb.
    bool button(const char* id, const char* label, OnPress cb);

    /// Числовой контрол. Значение приходит в args.value → @p cb.
    bool number(const char* id, const char* label, float min, float max,
                float step, const char* unit, OnNumber cb);

    /// Выпадающий список. @p options — массив C-строк продукта
    /// (копируются внутрь, до 6 опций по 15 символов).
    bool select(const char* id, const char* label,
                const char* const* options, uint8_t count, OnSelect cb);

    /// Ряд заводской разметки (слой 2): до 4 id сущностей.
    bool layoutRow(const char* a, const char* b = nullptr,
                   const char* c = nullptr, const char* d = nullptr);

    /// Есть ли что публиковать сверх авто-сенсоров.
    bool hasDeclarations() const { return count_ > 0 || layoutRows_ > 0; }

    /// Декларация менялась с последней публикации (loop перепубликует retained).
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    /// Собрать манифест: авто-сенсоры из cfg.has* + объявленные + layout.
    void buildJson(JsonDocument& doc, const iDryer::Config& cfg) const;

    /// Роутинг invoke: action == "card.{id}" → колбэк. true если обработано.
    bool handleInvokeAction(const char* action, JsonObjectConst args);

private:
    enum Kind : uint8_t { SensorK, BinaryK, ButtonK, NumberK, SelectK };

    struct Entity {
        Kind  kind;
        char  id[24];
        char  label[32];
        char  unit[8];
        char  path[48];
        char  deviceClass[20];
        float numMin = 0, numMax = 100, numStep = 1;
        static constexpr uint8_t MAX_OPTS = 6;
        static constexpr uint8_t OPT_LEN  = 15;
        char    options[MAX_OPTS][OPT_LEN + 1] = {{0}};
        uint8_t optCount = 0;
        OnPress  onPress  = nullptr;
        OnNumber onNumber = nullptr;
        OnSelect onSelect = nullptr;
    };

    static constexpr uint8_t MAX_ENTITIES  = 16;
    static constexpr uint8_t MAX_ROWS      = 8;
    static constexpr uint8_t MAX_ROW_ITEMS = 4;

    Entity  entities_[MAX_ENTITIES];
    uint8_t count_ = 0;

    char    layout_[MAX_ROWS][MAX_ROW_ITEMS][24];
    uint8_t rowLen_[MAX_ROWS] = {0};
    uint8_t layoutRows_ = 0;

    bool dirty_ = false;

    Entity* alloc_(Kind kind, const char* id, const char* label);
};

} // namespace idryer
