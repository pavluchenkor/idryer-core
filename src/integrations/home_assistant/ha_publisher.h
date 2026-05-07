/**
 * @file ha_publisher.h
 * @brief Публикация данных в Home Assistant с MQTT Discovery
 *
 * Поддерживает:
 * - Автоматическое создание сенсоров через Discovery
 * - Публикацию telemetry (температура, влажность, мощность)
 * - Публикацию status (режим работы)
 * - Публикацию events/alerts (ошибки, предупреждения)
 */

#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_mqtt_client.h"
#include "../../uart/uart_protocol.h"
#include <functional>

namespace idryer {
namespace ha {

/// @brief Какие sensor/binary_sensor entity публиковать в Discovery.
/// Флаги соответствуют iDryer::Config.hasXxx — продукт без датчика
/// не должен создавать entity, иначе HA UI показывает "Unknown".
struct HaCapabilities {
    bool airTemp     = true;
    bool airHumidity = true;
    bool heaterPower = true;
    bool fan         = true;
    bool weight      = false;
    /// mode и target_*/mode_control — общие управляющие entity, всегда.
};

class HaPublisher {
public:
    explicit HaPublisher(HaMqttClient* mqtt);

    bool publishDiscovery(const char* deviceId,
                          uint8_t unitsCount,
                          const char* hwVersion = "unknown",
                          const char* fwVersion = "unknown",
                          int tempMin = 30,
                          int tempMax = 90,
                          int durationMax = 1440,
                          const HaCapabilities& caps = HaCapabilities{});

    /// @brief Удалить retained Discovery конкретного sensor/binary_sensor.
    /// Используется когда capabilities изменились (например, выключен датчик).
    /// Без этого старый entity остаётся видимым в HA UI как "Unknown".
    bool removeSensorDiscovery(uint8_t unitId, const char* sensorName, bool binary = false);

    bool publishTelemetry(const idryer::UartTelemetryPayload& data);
    bool publishStatus(const idryer::UartStatusPayload& data);
    bool publishWeights(const idryer::UartWeightsPayload& data);
    bool publishAlert(uint8_t unitId, const char* message, const char* severity = "error");

    /**
     * @brief Универсальная публикация state одного юнита.
     *
     * Не зависит от UART payload — продукт передаёт значения напрямую.
     * Используется фасадом для авто-публикации state в HA параллельно
     * с публикацией в портал. Покрывает все entities, объявленные в
     * publishDiscovery: temperature, humidity, heater_power, fan, mode,
     * target_temp, target_duration.
     *
     * @param unitId       индекс юнита (0..3 → "U1".."U4")
     * @param temperatureC температура камеры °C (0 если нет датчика)
     * @param humidityPct  влажность % (0 если нет датчика)
     * @param heaterPowerPct мощность нагрева 0..100
     * @param fanOn        состояние вентилятора
     * @param modeStr      "IDLE"/"DRYING"/"STORAGE" — для select state
     * @param targetTempC  текущая целевая температура (для number target_temp)
     * @param targetDurMin текущая целевая длительность мин (для number target_duration)
     */
    bool publishUnitState(uint8_t unitId,
                          float temperatureC, float humidityPct,
                          int heaterPowerPct, bool fanOn,
                          const char* modeStr,
                          float targetTempC, uint32_t targetDurMin);

    using HaCommandCallback = std::function<void(const char* command, const char* unitId,
                                                  int temperature, int duration)>;
    void setCommandCallback(HaCommandCallback cb) { commandCallback_ = cb; }

    void resetDiscoveryPublished() { discoveryPublished_ = false; }
    bool isDiscoveryPublished() const { return discoveryPublished_; }

private:
    HaMqttClient* mqtt_;
    uint8_t unitsCount_ = 1;
    bool discoveryPublished_ = false;

    char deviceId_[32];
    char hwVersion_[16];
    char fwVersion_[16];
    int tempMin_ = 30;
    int tempMax_ = 90;
    int durationMax_ = 1440;

    static constexpr size_t HA_TOPIC_BUF_SIZE = 128;
    static constexpr size_t HA_PAYLOAD_BUF_SIZE = 1024;

    char topicBuf_[HA_TOPIC_BUF_SIZE];
    char payloadBuf_[HA_PAYLOAD_BUF_SIZE];

    HaCommandCallback commandCallback_;

    static constexpr uint8_t MAX_UNITS = 4;
    int16_t targetTempC_[MAX_UNITS]   = {55, 55, 55, 55};
    uint16_t targetDurMin_[MAX_UNITS] = {240, 240, 240, 240};

    bool publishSelectDiscovery(uint8_t unitId);
    bool publishNumberDiscovery(uint8_t unitId, const char* name, const char* cmdSuffix,
                                const char* stateSuffix, int min, int max,
                                const char* unit, const char* icon);
    bool subscribeToCommands();
    void handleIncomingMessage(const char* topic, const char* payload);

    bool publishSensorDiscovery(uint8_t unitId, const char* sensorName,
                                const char* deviceClass, const char* unit,
                                const char* icon = nullptr);

    bool publishBinarySensorDiscovery(uint8_t unitId, const char* sensorName,
                                       const char* deviceClass = nullptr,
                                       const char* icon = nullptr);

    bool publishAlertDiscovery();

    void makeStateTopic(char* buf, size_t size, uint8_t unitId, const char* sensorName);
    void makeConfigTopic(char* buf, size_t size, const char* domain,
                         uint8_t unitId, const char* sensorName);
    void makeDeviceJson(char* buf, size_t size);

    static void formatUnitId(char* buffer, size_t size, uint8_t unitId);
    static const char* modeToString(idryer::UartDryerMode mode);
};

} // namespace ha
} // namespace idryer

#endif // ESP32
