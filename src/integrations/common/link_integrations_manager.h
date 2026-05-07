/**
 * @file link_integrations_manager.h
 * @brief Orchestrator for LINK printer integrations: Home Assistant, Bambu Lab, Moonraker.
 *
 * Handles two commands dispatched through the product's command handler:
 *   - @c commands/link_integration — configure or switch the active integration
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

#include "link_integrations_types.h"
#include "link_integrations_store.h"
#include "../bambu/bambu_client.h"
#include "../moonraker/moonraker_client.h"
#include "../home_assistant/ha_integration_adapter.h"
#include "../home_assistant/ha_publisher.h"
#include "../../uart/uart_protocol.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "../../mqtt/mqtt_client.h"
#include <ArduinoJson.h>

namespace idryer {
namespace cloud {

/**
 * @brief Manages one active printer integration at a time (HA, Bambu, or Moonraker).
 *
 * Only one integration can be active at a time. Switching is done via
 * @c handleLinkIntegrationCommand() with @c {"active": "ha"|"bambu"|"moonraker"|"none"}.
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
     * Also handles @c {"active": "..."} to switch the active integration.
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
     * @brief Switches the active integration and saves the choice to NVS.
     *
     * Tears down the previously active client and starts the new one.
     */
    void setActive(ActiveIntegration active);

    /// @brief Returns the currently active integration.
    ActiveIntegration getActive() const { return selection_.active; }

    /**
     * @brief Wires HA commands (set_temp/set_duration/set_mode) to product-side dispatch.
     *
     * HaPublisher already accumulates pending temp/duration internally and converts
     * `set_mode IDLE/DRYING/STORAGE` into the unified command vocabulary
     * (`stop`/`drying`/`storage`) — same as the portal MQTT contract. This method
     * lets the facade subscribe a single callback that routes those commands into
     * the same dispatch path the portal uses, so HA and the portal share one code
     * path on the product side.
     */
    void setHaCommandCallback(ha::HaPublisher::HaCommandCallback cb) {
        haPublisher_.setCommandCallback(std::move(cb));
    }

    /// @brief Must be called every iteration of the main loop.
    void loop();

    /// @brief Forces a rebuild and publish of @c integrations/status.
    void publishStatus();

    // ── Moonraker callbacks ───────────────────────────────────────────────────

    /// @brief Called when Moonraker sets a new virtual chamber target temperature.
    void setChamberTargetCallback(MoonrakerClient::ChamberTargetCallback cb);

    /// @brief Called when the Moonraker connection state changes.
    void setMoonrakerStatusCallback(MoonrakerClient::StatusChangeCallback cb);

    /// @brief Called when Moonraker sends virtual chamber data.
    void setVirtualChamberCallback(MoonrakerClient::VirtualChamberCallback cb);

    const MoonrakerStatus&    moonrakerStatus()    const { return moonrakerClient_.status(); }
    MoonrakerConnectionState  moonrakerState()     const { return moonrakerClient_.state();  }
    const char*               moonrakerLastError() const { return moonrakerClient_.lastError(); }
    const MoonrakerConfig&    moonrakerConfig()    const { return moonraker_; }

    // ── Bambu callbacks ───────────────────────────────────────────────────────

    /// @brief Called when the Bambu printer status changes.
    void setBambuPrinterStatusCallback(BambuClient::PrinterStatusCallback cb);

    const BambuPrinterStatus& bambuPrinterStatus() const { return bambuClient_.printerStatus(); }
    BambuConnectionState      bambuState()         const { return bambuClient_.state();  }
    const char*               bambuLastError()     const { return bambuClient_.lastError(); }
    const BambuConfig&        bambuConfig()        const { return bambu_; }

    // ── Home Assistant ────────────────────────────────────────────────────────

    /// @brief Sets the MQTT client ID used for the HA MQTT connection.
    void setHaClientId(const char* clientId) { haClient_.setClientId(clientId); }

    /**
     * @brief Passes device identity info for HA MQTT Discovery payloads.
     *
     * Call once before @c begin(), typically from @c Link::begin() after loading
     * the device identity. If not called, discovery will be skipped.
     */
    void setDeviceInfo(const char* deviceId, uint8_t unitsCount,
                       const char* hwVersion = "unknown",
                       const char* fwVersion = "unknown");

    ha::HaMqttClient* haMqttClient() { return haClient_.mqttClient(); }

    HaConnectionState haState()         const { return haClient_.state();  }
    const char*       haLastError()     const { return haClient_.lastError(); }
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

    static void copyField(JsonObjectConst data, const char* key, char* buf, size_t bufSize);

    idryer::MqttClient*      mqtt_;
    LinkIntegrationsStore*   store_;

    BambuClient             bambuClient_;
    MoonrakerClient         moonrakerClient_;
    HaIntegrationAdapter    haClient_;
    ha::HaPublisher         haPublisher_;

    UartDeviceType          deviceType_ = UartDeviceType::Dryer;

    char    haDeviceId_[48]  = {0};
    uint8_t haUnitsCount_    = 1;
    char    haHwVersion_[16] = {0};
    char    haFwVersion_[16] = {0};

    HaConfig         ha_;
    BambuConfig      bambu_;
    MoonrakerConfig  moonraker_;
    CommonConfig     selection_;

    char haLastError_[96]        = {0};
    char bambuLastError_[96]     = {0};
    char moonrakerLastError_[96] = {0};

    static constexpr uint32_t kPeriodicStatusIntervalMs = 30000;
    uint32_t lastStatusPublishMs_ = 0;

    bool    bambuHasLastApply_         = false;
    char    bambuLastApplyAt_[24]      = {0};
    char    bambuLastApplyResult_[16]  = {0};
    char    bambuLastApplySpoolId_[40] = {0};
    uint8_t bambuLastApplyAmsId_       = 0;
    uint8_t bambuLastApplyTrayId_      = 0;

    void applyActiveIntegration();
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
