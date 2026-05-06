/**
 * @file bambu_client.h
 * @brief Клиент Bambu Lab LAN MQTT.
 *
 * LINK держит отдельное MQTT-подключение к принтеру Bambu в локальной
 * сети пользователя (TLS, порт 8883, self-signed cert — `setInsecure`,
 * username `bblp`, password = LAN Access Code). Через это подключение:
 *
 *  - Writer-режим (iDryer, deviceType == Dryer):
 *    `applyFilament(...)` → MQTT publish `ams_filament_setting`
 *    в `device/<printerSerial>/request`.
 *
 *  - Reader-режим (iHeater, deviceType == Heater/IHeaterLink):
 *    подписка на `device/<printerSerial>/report`.
 *
 * @see docs/ru/07-features/05-bambu-integration.md
 */

#pragma once

#include "../common/link_integrations_types.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <functional>

namespace idryer {
namespace cloud {

/// Runtime-состояние `BambuClient` (не путать с общим `IntegrationState`,
/// который описывает секцию в `integrations/status`).
enum class BambuConnectionState : uint8_t
{
    Disabled,       // begin() не был вызван
    Idle,           // настроен, но не подключается (enabled=false)
    Connecting,     // идёт connect / reconnect с backoff
    Connected,      // MQTT сессия активна
    Error,          // фатальная ошибка (auth rejected и т.п.)
};

const char* bambuConnectionStateToString(BambuConnectionState value);

/// Результат `applyFilament`.
struct BambuApplyResult
{
    bool success = false;
    char errorMessage[96] = {0};  // пусто если success=true
};

/// Снимок статуса принтера Bambu из периодического `device/<serial>/report`.
/// Заполняется по мере получения отчётов; поля, которые принтер ещё
/// не прислал, остаются в дефолте.
struct BambuPrinterStatus
{
    char     gcodeState[16]   = {0};   // "IDLE"/"PREPARE"/"RUNNING"/"PAUSE"/"FINISH"/"FAILED"
    uint8_t  progressPercent  = 0;     // mc_percent (0..100)
    uint32_t remainingSeconds = 0;     // mc_remaining_time (минуты → сек)
    uint16_t currentLayer     = 0;     // layer_num
    uint16_t totalLayers      = 0;     // total_layer_num
    float    nozzleTemp       = 0.0f;  // nozzle_temper
    float    nozzleTarget     = 0.0f;  // nozzle_target_temper
    float    bedTemp          = 0.0f;  // bed_temper
    float    bedTarget        = 0.0f;  // bed_target_temper
    float    chamberTemp      = 0.0f;  // chamber_temper (X1C)
    float    chamberTarget    = 0.0f;  // chamber_target (X1C; 0 если нет)

    // Текущий активный филамент (для Reader-сценария iHeater).
    char     trayType[16]     = {0};   // "PLA", "PETG", ...
    char     trayInfoIdx[8]   = {0};
};

/// Режим работы клиента Bambu — определяется по `deviceType` устройства.
enum class BambuMode : uint8_t
{
    Writer,   // Dryer: активен Writer-путь (applyFilament), report не читается
    Reader,   // Heater/IHeaterLink: активен Reader-путь (subscribe report, колбэк наружу)
};

class BambuClient
{
public:
    using StateChangeCallback  = std::function<void(BambuConnectionState)>;
    using PrinterStatusCallback = std::function<void(const BambuPrinterStatus&)>;

    BambuClient();
    ~BambuClient();

    // Жизненный цикл -----------------------------------------------------------

    /// Режим работы (Writer/Reader). По умолчанию Writer (для Dryer).
    void setMode(BambuMode mode);
    BambuMode mode() const { return mode_; }

    /// Применить конфигурацию. Если `cfg.enabled == false` — клиент
    /// остаётся в `Idle` без попыток подключения. Если конфиг уже
    /// идентичен — no-op.
    void configure(const BambuConfig& cfg);

    /// Остановить клиента и освободить TLS-ресурсы.
    void shutdown();

    /// Вызывать из главного цикла: обслуживает reconnect и PubSubClient.loop().
    void loop();

    /// Состояние подключения.
    BambuConnectionState state() const { return state_; }
    bool isConnected() const { return state_ == BambuConnectionState::Connected; }

    /// Последняя ошибка (пусто если всё ок).
    const char* lastError() const { return lastError_; }

    /// Колбэк при смене состояния (для обновления `integrations/status`).
    void setStateChangeCallback(StateChangeCallback cb) { stateCallback_ = std::move(cb); }

    /// Колбэк при обновлении статуса принтера (Reader-режим).
    void setPrinterStatusCallback(PrinterStatusCallback cb) { printerStatusCallback_ = std::move(cb); }

    /// Последний снимок статуса принтера.
    const BambuPrinterStatus& printerStatus() const { return printerStatus_; }

    // Writer API --------------------------------------------------------------

    /// Послать `ams_filament_setting` в принтер.
    BambuApplyResult applyFilament(const BambuApplyPayload& payload);

private:
    static constexpr uint32_t kReconnectMinMs  = 1000;
    static constexpr uint32_t kReconnectMaxMs  = 60000;
    static constexpr uint16_t kBambuPort       = 8883;
    static constexpr const char* kBambuUser    = "bblp";
    static constexpr uint16_t kTlsBufferSize   = 4096;

    void setState(BambuConnectionState newState);
    void setError(const char* message);
    void attemptConnect();
    bool publishJsonDoc(const char* topic, const String& payload);

    void handleReportMessage(const char* topic, const uint8_t* payload, unsigned int length);
    void requestPushAll();

    BambuConfig             cfg_{};
    bool                    configValid_ = false;
    BambuConnectionState    state_       = BambuConnectionState::Disabled;
    BambuMode               mode_        = BambuMode::Writer;
    char                    lastError_[96] = {0};

    WiFiClientSecure        tlsClient_;
    PubSubClient            mqttClient_{tlsClient_};

    uint32_t                reconnectBackoffMs_ = kReconnectMinMs;
    uint32_t                nextConnectAttemptMs_ = 0;

    StateChangeCallback     stateCallback_;
    PrinterStatusCallback   printerStatusCallback_;

    char                    requestTopic_[48] = {0};
    char                    reportTopic_[48]  = {0};

    BambuPrinterStatus      printerStatus_{};

    // PubSubClient принимает сырой function pointer. Статический мост
    // маршрутизирует на текущий инстанс клиента. Ограничение: один
    // BambuClient одновременно — у нас так и есть, в LinkIntegrationsManager.
    static BambuClient*     instance_;
    static void             staticMqttCallback(char* topic, uint8_t* payload, unsigned int length);
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
