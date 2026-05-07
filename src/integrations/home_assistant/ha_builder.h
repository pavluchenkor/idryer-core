/**
 * @file ha_builder.h
 * @brief Generic HA Discovery helpers + command-routing wrapper.
 *
 * Продукт регистрирует свои HA controls (button / number / select) с
 * собственными именами и колбэками, библиотека сама:
 *   - публикует config-топик на HA-брокер при коннекте;
 *   - подписывается на command_topic;
 *   - вызывает продуктовый колбэк при поступлении нажатия / значения.
 *
 * Никаких зашитых "drying / storage / IDLE" — что хочет продукт, то и
 * объявляет. Storage публикует свои анимации, iHeater Link — свои
 * температурные кнопки.
 */

#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_publisher.h"
#include "ha_mqtt_client.h"
#include <functional>
#include <stdint.h>

namespace idryer {
namespace ha {

class HaBuilder {
public:
    HaBuilder(HaPublisher* pub, HaMqttClient* mqtt);

    /// Вызывается либой когда identity (deviceId) известен — до publish.
    void setDeviceId(const char* deviceId);

    using OnPress  = std::function<void()>;
    using OnNumber = std::function<void(int value)>;
    using OnSelect = std::function<void(const char* option)>;

    /// Кнопка в HA UI. При нажатии вызывается @p cb.
    void button(const char* id, const char* name, OnPress cb,
                const char* icon = nullptr);

    /// Числовое поле в HA UI (slider/spinner). При изменении значения — @p cb.
    void number(const char* id, const char* name, int min, int max,
                OnNumber cb, const char* unit = nullptr, const char* icon = nullptr,
                int step = 1);

    /// Выпадающий список. При выборе опции — @p cb с её строкой.
    /// @p options — null-terminated array of C-strings.
    void select(const char* id, const char* name,
                const char* const* options, uint8_t count,
                OnSelect cb, const char* icon = nullptr);

    /// Вызывается из либы при подключении к HA-брокеру — публикует все
    /// зарегистрированные controls и подписывается на их command-топики.
    /// Продукту звать не нужно.
    void republishAll();

    /// Вызывается либой при входящем сообщении на HA-брокере. Маршрутизирует
    /// в зарегистрированный onPress/onNumber/onSelect.
    void handleIncoming(const char* topic, const char* payload);

private:
    enum Kind : uint8_t { ButtonK, NumberK, SelectK };

    struct Entry {
        Kind kind;
        char id[24];
        char name[32];
        char icon[24];
        // For Number:
        int  numMin = 0, numMax = 100, numStep = 1;
        char unit[8] = {0};
        // For Select: до 6 опций по 12 символов.
        static constexpr uint8_t MAX_OPTS = 6;
        static constexpr uint8_t OPT_LEN  = 12;
        char options[MAX_OPTS][OPT_LEN] = {{0}};
        uint8_t optCount = 0;
        // Callbacks — один из них активен по kind.
        OnPress  onPress;
        OnNumber onNumber;
        OnSelect onSelect;
    };

    static constexpr uint8_t MAX_ENTRIES = 8;
    Entry   entries_[MAX_ENTRIES];
    uint8_t count_ = 0;

    HaPublisher*  pub_;
    HaMqttClient* mqtt_;
    char          deviceId_[48] = {0};

    void publishOne(const Entry& e);
    void buildCmdTopic(const Entry& e, char* out, size_t outSize);
};

} // namespace ha
} // namespace idryer

#endif // ESP32
