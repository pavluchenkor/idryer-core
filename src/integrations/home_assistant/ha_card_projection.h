/**
 * @file ha_card_projection.h
 * @brief Home Assistant из card-манифеста устройства.
 *
 * Discovery строится по манифесту: сущности — по путям в телеметрии, действия —
 * кнопками, их параметры — полями number / select / text с пределами юнита.
 * Значения HA берёт из копии telemetry / status / weights, которую ядро шлёт на
 * HA-брокер. Нажатие кнопки — invoke card.{id} с текущими значениями полей тем
 * же путём, что вызов из локальной сети. Своего списка сущностей у продукта
 * нет: что на карточке, то и в HA.
 *
 * Топики и соглашения: mqtt_contract.yaml → ha_integration_topics,
 * ha_discovery_topics.
 */
#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <ArduinoJson.h>
#include "ha_mqtt_client.h"

namespace idryer {
namespace ha {

class HaCardProjection {
public:
    /// Исполнить команду invoke ({action, unitId, args}) обычным путём ядра.
    using InvokeFn = void (*)(void* ctx, JsonObjectConst command);
    /// Собрать card-манифест в @p doc; вернуть число юнитов.
    using ManifestFn = uint8_t (*)(void* ctx, JsonDocument& doc);
    /// Discovery опубликован — продукт/ядро шлёт свежие значения.
    using PublishedFn = void (*)(void* ctx);

    explicit HaCardProjection(HaMqttClient* mqtt) : mqtt_(mqtt) {}

    /// Идентичность в HA. Вызывать до подключения к брокеру: ставит LWT.
    void setDevice(const char* serial, const char* model, const char* fw, const char* hw);
    /// Прежний идентификатор прибора в HA (старые версии ядра брали серийник
    /// контроллера) — его конфиги тоже убираются.
    void setLegacyId(const char* id);
    void setInvokeHandler(InvokeFn fn, void* ctx) { invokeFn_ = fn; invokeCtx_ = ctx; }
    void setManifestSource(ManifestFn fn, void* ctx) { manifestFn_ = fn; manifestCtx_ = ctx; }
    void setPublishedHandler(PublishedFn fn, void* ctx) { publishedFn_ = fn; publishedCtx_ = ctx; }

    /// Брокер HA подключён / отключён.
    void onConnected();
    void onDisconnected();
    /// Манифест изменился (пределы из меню, число юнитов, декларация).
    void requestRepublish();

    /// Копия исходящих telemetry / status / weights — state_topic сущностей.
    void mirror(const char* kind, JsonDocument& doc);

    /// Входящее с брокера HA. Здесь ничего не публикуется: буфер клиента общий
    /// для входящих и исходящих, всё исполняет loop().
    void handleIncoming(const char* topic, const char* payload);
    /// Работа порциями: цикл продукта не должен стоять — иначе переполнится
    /// приём UART от контроллера.
    void loop();

private:
    enum Kind : uint8_t {
        ParamNumber, ParamSelect, ParamColor, ActionButton,
        EntityNumber, EntitySelect, EntityButton,
    };
    enum Comp : uint8_t { CompSensor, CompBinary, CompNumber, CompSelect, CompText, CompButton };
    enum Phase : uint8_t { PhIdle, PhEntities, PhModes, PhActions };

    struct Control {
        char    object[40];
        Kind    kind;
        uint8_t unit;
        char    action[24];   // id действия или сущности-контрола
        char    param[24];    // id параметра действия
        float   min, max, step, num;
        char    str[16];
        bool    stateDirty;   // значение сменилось — опубликовать state
        bool    live;         // есть в последнем манифесте
    };
    struct Published {
        char object[40];
        Comp comp;
        bool seen;
    };

    static constexpr uint8_t  MAX_CONTROLS  = 40;
    static constexpr uint8_t  MAX_PUBLISHED = 64;
    static constexpr uint8_t  MAX_PENDING   = 4;
    static constexpr uint8_t  MAX_STALE     = 48;
    static constexpr uint8_t  PER_LOOP      = 2;       // публикаций за проход loop()
    static constexpr uint32_t CLEANUP_MS    = 10000;
    static constexpr uint32_t SETTLE_MS     = 2000;

    HaMqttClient* mqtt_;
    InvokeFn    invokeFn_     = nullptr;
    void*       invokeCtx_    = nullptr;
    ManifestFn  manifestFn_   = nullptr;
    void*       manifestCtx_  = nullptr;
    PublishedFn publishedFn_  = nullptr;
    void*       publishedCtx_ = nullptr;

    char serial_[32]   = {0};
    char legacyId_[32] = {0};
    char model_[32]    = {0};
    char fw_[16]       = {0};
    char hw_[16]       = {0};

    bool     connected_      = false;
    bool     needPublish_    = false;
    uint32_t publishAfterMs_ = 0;
    bool     subscribed_     = false;
    uint32_t cleanupUntilMs_ = 0;

    // Проход публикации: манифест в куче и курсор по нему.
    DynamicJsonDocument* doc_ = nullptr;
    Phase   phase_   = PhIdle;
    uint8_t units_   = 1;
    bool    hasModes_ = false;
    uint8_t ci_ = 0, cu_ = 0, ck_ = 0;
    // Уборка прежнего формата: курсор по известным именам.
    uint8_t legacyStep_ = 0xFF;

    Control   controls_[MAX_CONTROLS];
    uint8_t   controlCount_ = 0;
    Published published_[MAX_PUBLISHED];
    uint8_t   publishedCount_ = 0;
    uint8_t   pending_[MAX_PENDING];
    uint8_t   pendingCount_ = 0;
    Published stale_[MAX_STALE];   // кандидаты на удаление (seen не используется)
    uint8_t   staleCount_ = 0;
    char      buf_[1600];

    void startPass_();
    bool emitNext_();
    void finishPass_();
    void emitEntity_(JsonObjectConst e, uint8_t u);
    uint8_t entityItems_(JsonObjectConst e) const;
    void emitMode_(uint8_t u);
    bool emitParam_(JsonObjectConst a, JsonObjectConst p, uint8_t u);
    void emitButton_(JsonObjectConst a, uint8_t u);
    bool emitLegacy_();

    void sendConfig_(Comp comp, const char* object, JsonDocument& cfg);
    void publishState_(const Control& c);
    void execute_(const Control& c);
    Control* control_(const char* object);
    Control* upsertControl_(const char* object, Kind kind, uint8_t unit,
                            const char* action, const char* param);
    bool isPublished_(const char* object) const;
    void queueStale_(const char* object, Comp comp);
    void topic_(char* out, size_t n, const char* object, const char* leaf) const;
};

} // namespace ha
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
