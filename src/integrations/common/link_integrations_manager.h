/**
 * @file link_integrations_manager.h
 * @brief Orchestrator for LINK printer integrations: Home Assistant, Bambu Lab, Moonraker.
 *
 * Handles two commands dispatched through the product's command handler:
 *   - @c commands/link_integration — configure and switch integrations on or off
 *   - @c commands/bambu_apply      — apply filament profile to a Bambu printer AMS slot
 *
 * Stores integration config in NVS via @c LinkIntegrationsStore and publishes
 * connection state to @c idryer/{serial}/integrations/status.
 *
 * Usage — dispatch inside the product's handleCommand() in @c setup():
 * @code
 * #include <idryer_integrations.h>
 *
 * LinkIntegrationsStore intStore;
 * idryer::cloud::LinkIntegrationsManager intManager(&mqtt, &intStore);
 *
 * static void handleCommand(const char* cmd, JsonObjectConst data) {
 *     if (strcmp(cmd, "link_integration") == 0) {
 *         intManager.handleLinkIntegrationCommand(data); return;
 *     }
 *     if (strcmp(cmd, "bambu_apply") == 0) {
 *         intManager.handleBambuApplyCommand(data); return;
 *     }
 *     // ... other commands ...
 * }
 *
 * runtime.setCommandHandler(handleCommand);
 * local.setCommandSink(handleCommand);
 * intManager.begin(); // after runtime.begin()
 * // in loop(): intManager.loop();
 * @endcode
 */

#pragma once

#include "integration_features.h"
#include "link_integrations_types.h"
#include "link_integrations_store.h"
#include "../bambu/bambu_client.h"
#include "../moonraker/moonraker_client.h"
#include "../home_assistant/ha_integration_adapter.h"
#include "../home_assistant/ha_card_projection.h"
#include "../../uart/uart_protocol.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "../../mqtt/mqtt_client.h"
#include <ArduinoJson.h>

namespace idryer {
namespace cloud {

/**
 * @brief Manages the device integrations: Home Assistant plus one printer.
 *
 * Home Assistant is independent — it is a control and reporting channel, not an
 * alternative to the printer, so it runs in parallel and is switched by its own
 * @c ha.enabled flag.
 *
 * Bambu and Moonraker are mutually exclusive: the device has a single printer.
 * The choice lives in @c selection_.active and follows the @c enabled field of
 * their sections in @c commands/link_integration.
 */
class LinkIntegrationsManager
{
public:
    /**
     * @param mqtt  The device MQTT client (used to publish @c integrations/status).
     * @param store NVS-backed storage for all integration configs.
     */
    LinkIntegrationsManager(idryer::MqttClient* mqtt, LinkIntegrationsStore* store);

    /**
     * @brief Loads all integration configs from NVS and starts the active integration.
     *
     * Call once after @c runtime.begin().
     */
    void begin();

    /**
     * @brief Handles the @c commands/link_integration MQTT command.
     *
     * Dispatches by the @c "type" field in @p data:
     *   - @c "ha"        — updates Home Assistant config (host, port, token)
     *   - @c "bambu"     — updates Bambu Lab config (host, serial, access code)
     *   - @c "moonraker" — updates Moonraker config (host, port, API key)
     *
     * The @c "enabled" field of a section is the switch: for Home Assistant it
     * starts or stops HA, for a printer section it also becomes the active
     * printer integration (turning the other one off).
     */
    void handleLinkIntegrationCommand(JsonObjectConst data);

    /**
     * @brief Handles the @c commands/bambu_apply MQTT command.
     *
     * Applies filament settings to a specific AMS slot on the connected Bambu printer.
     * Only does something if Bambu is the active integration and the printer is connected.
     */
    void handleBambuApplyCommand(JsonObjectConst data);

    /**
     * @brief Switches the active PRINTER integration and saves the choice to NVS.
     *
     * Tears down the previously active client and starts the new one.
     * @c ActiveIntegration::Ha is accepted for backwards compatibility and is
     * redirected to @c setHaEnabled(true) without touching the printer.
     */
    void setActive(ActiveIntegration active);

    /// @brief Returns the currently active printer integration.
    ActiveIntegration getActive() const { return selection_.active; }

    /// @brief Turns Home Assistant on or off, independently of the printer.
    void setHaEnabled(bool enabled);

    /// @brief Is Home Assistant switched on?
    bool haEnabled() const { return ha_.enabled; }

    /// Сущности HA строит генератор из card-манифеста: менеджер передаёт ему
    /// смену соединения и входящие сообщения брокера HA.
    void setHaProjection(ha::HaCardProjection* projection) { haCard_ = projection; }

    /// @brief Must be called every iteration of the main loop.
    void loop();

    /// @brief Forces a rebuild and publish of @c integrations/status.
    void publishStatus();

    // ── Moonraker callbacks ───────────────────────────────────────────────────

    /// @brief Called when Moonraker sets a new virtual chamber target temperature.
    void setChamberTargetCallback(MoonrakerClient::ChamberTargetCallback::FnPtr fn, void* ctx = nullptr);

    /// @brief Called when the Moonraker connection state changes.
    void setMoonrakerStatusCallback(MoonrakerClient::StatusChangeCallback::FnPtr fn, void* ctx = nullptr);

    /// @brief Called when Moonraker sends virtual chamber data.
    void setVirtualChamberCallback(MoonrakerClient::VirtualChamberCallback::FnPtr fn, void* ctx = nullptr);

#if IDRYER_WITH_MOONRAKER
    const MoonrakerStatus&    moonrakerStatus()    const { return moonrakerClient_.status(); }
    MoonrakerConnectionState  moonrakerState()     const { return moonrakerClient_.state();  }
    const char*               moonrakerLastError() const { return moonrakerClient_.lastError(); }
#else
    const MoonrakerStatus&    moonrakerStatus()    const { static const MoonrakerStatus s{}; return s; }
    MoonrakerConnectionState  moonrakerState()     const { return MoonrakerConnectionState::Disabled; }
    const char*               moonrakerLastError() const { return ""; }
#endif
    const MoonrakerConfig&    moonrakerConfig()    const { return moonraker_; }

    // ── Bambu callbacks ───────────────────────────────────────────────────────

    /// @brief Called when the Bambu printer status changes.
    void setBambuPrinterStatusCallback(BambuClient::PrinterStatusCallback::FnPtr fn, void* ctx = nullptr);

#if IDRYER_WITH_BAMBU
    const BambuPrinterStatus& bambuPrinterStatus() const { return bambuClient_.printerStatus(); }
    BambuConnectionState      bambuState()         const { return bambuClient_.state();  }
    const char*               bambuLastError()     const { return bambuClient_.lastError(); }
#else
    const BambuPrinterStatus& bambuPrinterStatus() const { static const BambuPrinterStatus s{}; return s; }
    BambuConnectionState      bambuState()         const { return BambuConnectionState::Disabled; }
    const char*               bambuLastError()     const { return ""; }
#endif
    const BambuConfig&        bambuConfig()        const { return bambu_; }

    /// Включает/выключает логирование сырых payload'ов на обоих клиентах.
    void setLogPayloads(bool enabled) {
#if IDRYER_WITH_BAMBU
        bambuClient_.setLogPayloads(enabled);
#endif
#if IDRYER_WITH_MOONRAKER
        moonrakerClient_.setLogPayloads(enabled);
#endif
        (void)enabled;
    }

    // ── Home Assistant ────────────────────────────────────────────────────────

    /// @brief Sets the MQTT client ID used for the HA MQTT connection.
#if IDRYER_WITH_HA
    void setHaClientId(const char* clientId) { haClient_.setClientId(clientId); }

    ha::HaMqttClient* haMqttClient() { return haClient_.mqttClient(); }

    HaConnectionState haState()         const { return haClient_.state();  }
    const char*       haLastError()     const { return haClient_.lastError(); }
#else
    void setHaClientId(const char* clientId) { (void)clientId; }

    ha::HaMqttClient* haMqttClient() { return nullptr; }

    HaConnectionState haState()         const { return HaConnectionState::Disabled; }
    const char*       haLastError()     const { return ""; }
#endif
    const HaConfig&   haConfig()        const { return ha_; }

    // ── Device type ───────────────────────────────────────────────────────────

    /**
     * @brief Sets the device type so integrations can adapt their behavior.
     *
     * For example, Bambu integration behaves differently for iHeater (Reader)
     * vs iDryer (Writer). Default is @c UartDeviceType::Dryer.
     */
    void setDeviceType(UartDeviceType deviceType);

    UartDeviceType deviceType() const { return deviceType_; }

private:
    void serializeHaSection(JsonObject section) const;
    void serializeBambuSection(JsonObject section) const;
    void serializeMoonrakerSection(JsonObject section) const;

    // Текущее состояние секции для поля `state` в integrations/status.
    // Вычисляется из `selection_.active` и `configured()`:
    //   - active != эта секция               → Disabled
    //   - active == эта секция + !configured → ConfigMissing
    //   - active == эта секция + configured  → клиент сам выставляет
    //                                          Connecting / Online / Error.
    IntegrationState computeHaState() const;
    IntegrationState computeBambuState() const;
    IntegrationState computeMoonrakerState() const;

    bool parseHa(JsonObjectConst data, HaConfig& out) const;
    bool parseBambu(JsonObjectConst data, BambuConfig& out) const;
    bool parseMoonraker(JsonObjectConst data, MoonrakerConfig& out) const;
    bool parseBambuApply(JsonObjectConst data, BambuApplyPayload& out) const;

    /// Интеграция собрана в образ этой прошивки?
    static bool isSupported(ActiveIntegration kind);

    static void copyField(JsonObjectConst data, const char* key, char* buf, size_t bufSize);

    idryer::MqttClient*      mqtt_;
    LinkIntegrationsStore*   store_;

#if IDRYER_WITH_BAMBU
    BambuClient             bambuClient_;
#endif
#if IDRYER_WITH_MOONRAKER
    MoonrakerClient         moonrakerClient_;
#endif
#if IDRYER_WITH_HA
    HaIntegrationAdapter    haClient_;
#endif
    ha::HaCardProjection*   haCard_ = nullptr;

    UartDeviceType          deviceType_ = UartDeviceType::Dryer;

    HaConfig         ha_;
    BambuConfig      bambu_;
    MoonrakerConfig  moonraker_;
    CommonConfig     selection_;

    char haLastError_[96]        = {0};
    char bambuLastError_[96]     = {0};
    char moonrakerLastError_[96] = {0};

#if IDRYER_WITH_MOONRAKER
    // Последнее опубликованное состояние виртуальной камеры — по нему loop()
    // замечает, что данные Klipper доехали уже после публикации снимка.
    bool  vcAvailableSeen_ = false;
    bool  vcHasSensorSeen_ = false;
    float vcTargetSeen_    = 0.0f;
#endif

    // integrations/status — событийный: публикуется при изменении состояния/
    // конфига интеграций (retained+QoS1 хранят снапшот). Флаг — дошив
    // публикации, если событие случилось до MQTT-коннекта.
    bool statusPublishPending_ = false;

    bool    bambuHasLastApply_         = false;
    char    bambuLastApplyAt_[24]      = {0};
    char    bambuLastApplyResult_[16]  = {0};
    char    bambuLastApplySpoolId_[40] = {0};
    uint8_t bambuLastApplyAmsId_       = 0;
    uint8_t bambuLastApplyTrayId_      = 0;

    /// Включение принтерной интеграции гасит вторую; выключение освобождает выбор.
    void selectPrinter(ActiveIntegration kind, bool enabled);

    void applyIntegrations();
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
