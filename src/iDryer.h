/**
 * @file iDryer.h
 * @brief High-level facade for iDryer devices.
 *
 * Single-header public API. Hides the entire SDK stack (WiFi/Improv,
 * cloud state machine, HTTP claim, MQTT, UART, integrations, local-WS,
 * menu/NVS) behind one class @c iDryer::Link.
 *
 * Data types (enums, structs, Config) are auto-generated from
 * `mqtt_contract.yaml` into `_generated/iDryer_api.h` — this file only
 * declares the facade @c Link class and callback aliases.
 *
 * Typical usage:
 * @code
 * #include <iDryer.h>
 *
 * static const iDryer::Config CFG = {
 *     .deviceType        = iDryer::DeviceType::Dryer,
 *     .unitsCount        = 1,
 *     .hasAirTemp        = true,
 *     .hasAirHumidity    = true,
 *     .hasHeaterTemp     = true,
 *     .hasHeaterPower    = true,
 *     .hasFanStatus      = true,
 *     .hasScales         = false,
 *     .hasRfid           = true,
 *     .allowHa           = true,
 *     .allowBambu        = false,
 *     .allowMoonraker    = false,
 *     .telemetryPeriodMs = 1000,
 *     .statusPeriodMs    = 5000,
 *     .hardwareVersion   = "DRYER-v3",
 *     .firmwareVersion   = VERSION_STR,
 * };
 *
 * static iDryer::Link link(CFG);
 *
 * void setup() {
 *     link.begin();
 *     link.onRequest([](const iDryer::Request& r) {
 *         switch (r.kind) {
 *             case iDryer::RequestKind::Start:       myStart(r.unitId);       break;
 *             case iDryer::RequestKind::Stop:        myStop(r.unitId);        break;
 *             case iDryer::RequestKind::Find:        myFind(r.unitId);        break;
 *             case iDryer::RequestKind::ClearErrors: myClearErrors(r.unitId); break;
 *             default: break;
 *         }
 *     });
 * }
 *
 * void loop() {
 *     link.loop();
 *     link.telemetry.airTempC[0]   = mySensor.readTemp();
 *     link.status.targetTempC[0]   = myPid.setpoint();
 *     if (overheat) link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
 * }
 * @endcode
 */

#pragma once

#include <stdint.h>
#include <functional>
#include <ArduinoJson.h>

// Auto-generated from mqtt_contract.yaml — enums, data structs, Config.
// See contracts/gen_idryer_api_h.py.
#include "_generated/iDryer_api.h"

// Forward declaration for the SDK integrations manager — exposed via
// Link::integrationsManager() accessor for product-side wiring (auto_heat,
// menu lookup, etc.). Real definition is in <integrations/common/link_integrations_manager.h>.
namespace idryer { namespace cloud { class LinkIntegrationsManager; } }
namespace idryer { class MqttClient; class IdryerRuntime; class DevicePublisher; }
namespace idryer { namespace ha { class HaBuilder; } }

namespace iDryer {

// ──────────────────────────────────────────────────────────────────────
//  Link — the facade.
//  All data types (Telemetry/Status/Request/Config/enums) are declared
//  in _generated/iDryer_api.h. This class only adds API on top of them.
// ──────────────────────────────────────────────────────────────────────

class Link {
public:
    explicit Link(const Config& cfg);
    ~Link();

    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;

    /// Bring the entire stack up. Call once in setup().
    bool begin();

    /// Single mandatory tick. Call every iteration of Arduino loop().
    void loop();

    // ─── Outgoing data — user writes fields, SDK auto-publishes ──────
    Telemetry telemetry;
    Status    status;

    /// Force immediate publish (in addition to the periodic timer).
    void publishTelemetryNow();
    void publishStatusNow();

    // ─── Events — fire-and-forget, sent immediately ──────────────────
    /// Publishes to `idryer/{serial}/events`. Payload shape per contract:
    /// `{ severity, event, message, unitId, timestamp }`.
    ///
    /// @param severity  Info/Warning/Error → JSON `severity` (uppercase string).
    /// @param event     event code-string (e.g. "OVERHEAT", "SESSION_COMPLETE").
    /// @param message   free-form human-readable text.
    /// @param unitId    target unit (0..unitsCount-1) or 0xFF for device-wide.
    void raiseEvent(EventKind   severity,
                    const char* event,
                    const char* message,
                    uint8_t     unitId = 0xFF);

    // ─── Incoming command subscriptions ──────────────────────────────
    using CommandCallback           = std::function<void(JsonObjectConst data)>;
    using IntegrationStatusCallback = std::function<void(const IntegrationStatus&)>;
    using ClaimPinCallback          = std::function<void(const char* pin, uint32_t expiresInSeconds)>;
    using PublishHookCallback       = std::function<void(JsonObject root)>;

    /// Called right before telemetry is sent. Library has already filled
    /// the standard `units[]` array (capabilities-driven), `rssi`, `uptime`.
    /// Product can add product-specific top-level fields (e.g. iHeater Link
    /// adds `outputMode`/`targetTempC`/`active` for the portal). Returns
    /// JsonObject root — modify in place. Optional.
    void onTelemetryPublish(PublishHookCallback cb);

    /// Same as @ref onTelemetryPublish but for the `status` payload.
    void onStatusPublish(PublishHookCallback cb);

    // ─── Periodic tasks ──────────────────────────────────────────────
    using TaskCallback = std::function<void()>;
    using TaskHandle   = uint8_t;  ///< 0xFF = invalid

    /// Schedule @p cb to run every @p periodMs from `loop()`. Cooperative
    /// scheduler — callback runs in the same context as `loop()`, must
    /// not block. Returns handle for `cancel()`, or 0xFF if MAX_TASKS (8)
    /// is full. First call fires after the first period elapses, not
    /// immediately on registration.
    TaskHandle every(uint32_t periodMs, TaskCallback cb);

    /// Cancel a task previously scheduled with `every()`. Idempotent.
    void cancel(TaskHandle handle);

    /// Register a callback for a command name. The callback fires when MQTT
    /// or Local-WS receives `commands/{name}` with arbitrary JSON payload.
    ///
    /// Built-in commands (`link_integration`, `bambu_apply`, `ping`) are always
    /// handled by the library — these cannot be intercepted. But a product can
    /// still register a callback under the same name to run as a post-hook
    /// (e.g. iHeater Link's menu-toggle sync after `link_integration`).
    ///
    /// Common product names: `drying`, `stop`, `storage`, `get_config`, `set`,
    /// `profile`, `led.pulse` — anything is allowed. Returns false on overflow
    /// (max 12 commands) or invalid arguments.
    bool onCommand(const char* name, CommandCallback cb);

    /// Called when an integration changes connectivity state. Optional.
    void onIntegrationStatus(IntegrationStatusCallback cb);

    /// Called when the cloud claim flow produces a PIN.
    void onClaimPin(ClaimPinCallback cb);

    // ─── Diagnostics ─────────────────────────────────────────────────
    bool        isOnline() const;
    const char* serial() const;

    // ─── Dev / first-boot helpers ────────────────────────────────────
    /// If NVS has no WiFi credentials, save the given SSID/password.
    /// No-op if NVS already has creds. Call before begin().
    void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);

    /// Always overwrites WiFi credentials in NVS. Dev / forced re-provisioning.
    void setWifiCredentials(const char* ssid, const char* password);

    /// Manually start the cloud claim flow (provision → register → check-claim).
    /// Triggers @ref onClaimPin once the portal returns a PIN.
    bool requestClaim();

    /// Outlet to the SDK integrations manager — for product-side wiring of
    /// callbacks (Moonraker chamber target, Bambu printer status, etc.).
    /// Use this instead of duplicating mapping logic in the SDK; the SDK
    /// doesn't know product specifics like filament-type → temperature lookup.
    idryer::cloud::LinkIntegrationsManager* integrationsManager();

    /// Generic HA Discovery builder. Register your buttons / numbers / selects
    /// in `setup()`; the library publishes them on HA-connect and routes
    /// incoming commands to your callbacks.
    ///   link.ha().button("heat_50", "Heat 50", []{ startHeating(50); });
    ///   link.ha().select("anim", "Animation", opts, 4, [](const char* v) { ... });
    /// No-op when @c Config.allowHa is false.
    idryer::ha::HaBuilder& ha();

    /// Outlet to the SDK MQTT client — for product-side components that
    /// publish their own topics or hook into command routing (MenuBridge etc).
    idryer::MqttClient* mqttClient();

    /// Outlet to the SDK dual-publish helper — sends one payload to both MQTT
    /// and Local WS. Use this for product-side responses that must reach a
    /// LAN-only client (e.g. config response to `commands/get_config`) just like
    /// the facade's auto-published telemetry/status do.
    idryer::DevicePublisher* devicePublisher();

    /// Outlet to the SDK runtime — used when product needs to install its
    /// own full command handler (e.g. legacy iheaterlink::handleCommand
    /// covering get_config/set/drying via MenuBridge). Overrides the facade's
    /// default dispatch (onRequest/onProfile no longer fired by MQTT path).
    idryer::IdryerRuntime* runtime();

    /// Wipe NVS-stored device token (and identity) and reboot the chip.
    /// On next boot the device is unclaimed → auto-claim flow runs.
    void eraseClaimAndRestart();

private:
    struct Impl;
    Impl* impl_;

    /// File-scope pointer for non-capturing Improv callback.
    static Impl* s_currentImpl;

    void dispatchCommand(const char* command, JsonObjectConst data);
};

/// Returns the wire-format string for a DeviceType (e.g. "iheater_link").
const char* deviceTypeToString(DeviceType t);

} // namespace iDryer
