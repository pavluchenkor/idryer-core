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
 * Действия (v2) — операции прибора с параметрами запуска:
 *
 *   link.card().action("drying", "DRYING", onDrying)
 *       .param("temperature", "target_temperature", MENU_DRY_TEMP)
 *       .ceiling(MENU_AIR_MAX_TEMP)
 *       .param("duration", "duration", MENU_DRY_TIME);
 *
 * Параметр, привязанный к пункту меню, получает из него min/max/step и
 * значение по умолчанию для каждого юнита; меню читает мост
 * card_menu_bridge.h. Параметры операционные: колбэк получает их в args и
 * передаёт в команду запуска, в меню они не пишутся. Изменилось меню —
 * манифест перепубликуется сам (pollMenu).
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

    // ── Действия (v2) ────────────────────────────────────────────────────

    /// Колбэк действия. @p unit — индекс юнита (0..), @p args — значения
    /// параметров по их id: числа уже зажаты в limits юнита, пропущенные
    /// заполнены значением по умолчанию.
    using OnAction = void (*)(uint8_t unit, JsonObjectConst args);

    /// Пункт меню глазами карточки: пределы, шаг и текущее значение для юнита.
    struct MenuValue {
        float min = 0, max = 0, step = 1, value = 0;
        const char* unit = nullptr;
    };
    /// Чтение пункта меню. false — пункта нет или он не числовой.
    using MenuReader = bool (*)(uint16_t menuId, uint8_t unit, MenuValue& out);

    /// Подключить чтение меню (см. card_menu_bridge.h). Без него параметры,
    /// привязанные к меню, в манифест не попадают.
    void setMenuReader(MenuReader reader) { menuReader_ = reader; }

    static constexpr uint16_t NO_MENU = 0xFFFF;

    class ActionRef {
    public:
        /// Числовой параметр из пункта меню: limits/step/default/unit — из меню.
        ActionRef& param(const char* id, const char* purpose, uint16_t menuId);
        /// Числовой параметр со своими пределами (у меню такого пункта нет).
        ActionRef& param(const char* id, const char* purpose, float min, float max,
                         float step, float def, const char* unit = nullptr);
        /// Верхний предел последнего параметра ограничен значением пункта меню
        /// (потолок, например air_max_temp) для того же юнита.
        ActionRef& ceiling(uint16_t menuId);
        /// Параметр-список стадий профиля: [{temperature, ramp, hold}].
        ActionRef& stages(const char* id, const char* purpose = "stages");

    private:
        friend class CardBuilder;
        ActionRef(CardBuilder* b, int8_t idx) : b_(b), idx_(idx) {}
        CardBuilder* b_;
        int8_t idx_;   // -1 — действие не зарегистрировано, вызовы игнорируются
    };

    /// Действие: переводит юнит в режим @p mode (строка status.units[].mode).
    /// Остановка — действие с mode "IDLE". Нажатие на карточке → invoke
    /// card.{id} {args, unitId} → @p cb.
    ActionRef action(const char* id, const char* mode, OnAction cb);

    /// Сверить пункты меню, к которым привязаны параметры, с последней
    /// публикацией; изменились (или число юнитов) — манифест грязный.
    void pollMenu(uint8_t unitsCount);

    /// Ряд заводской разметки (слой 2): до 4 id сущностей.
    bool layoutRow(const char* a, const char* b = nullptr,
                   const char* c = nullptr, const char* d = nullptr);

    /// Есть ли что публиковать сверх авто-сенсоров.
    bool hasDeclarations() const { return count_ > 0 || layoutRows_ > 0 || actionCount_ > 0; }

    /// Декларация менялась с последней публикации (loop перепубликует retained).
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    /// Собрать манифест: авто-сенсоры из cfg.has* + объявленные + layout.
    void buildJson(JsonDocument& doc, const iDryer::Config& cfg) const;

    /// Роутинг invoke: action == "card.{id}" → колбэк. @p data — тело
    /// команды целиком ({action, args, unitId}). true если обработано.
    bool handleInvokeAction(const char* action, JsonObjectConst data, uint8_t unitsCount);

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

    enum ParamType : uint8_t { ParamNumber, ParamStages };

    struct Param {
        char     id[24];
        char     purpose[20];
        char     unit[8];
        ParamType type = ParamNumber;
        uint16_t menuId = NO_MENU;
        uint16_t ceilId = NO_MENU;
        float    min = 0, max = 0, step = 1, def = 0;
    };

#ifndef IDRYER_CARD_MAX_ACTIONS
#define IDRYER_CARD_MAX_ACTIONS 6
#endif
#ifndef IDRYER_CARD_MAX_PARAMS
#define IDRYER_CARD_MAX_PARAMS 4
#endif
    static constexpr uint8_t MAX_ACTIONS = IDRYER_CARD_MAX_ACTIONS;
    static constexpr uint8_t MAX_PARAMS  = IDRYER_CARD_MAX_PARAMS;

    struct Action {
        char     id[24];
        char     mode[20];
        Param    params[MAX_PARAMS];
        uint8_t  paramCount = 0;
        OnAction cb = nullptr;
    };

    Action     actions_[MAX_ACTIONS];
    uint8_t    actionCount_ = 0;
    MenuReader menuReader_ = nullptr;
    uint32_t   menuFingerprint_ = 0;

    Param* addParam_(int8_t actionIdx, const char* id, const char* purpose);
    /// Пределы, шаг и значение по умолчанию параметра для юнита.
    bool resolve_(const Param& p, uint8_t unit, float& mn, float& mx,
                  float& st, float& df, const char** unitStr) const;
    uint32_t fingerprint_(uint8_t unitsCount) const;

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
