#pragma once

#include "../cloud/cloud_state_machine.h"
#include "../cloud/action_dispatcher.h"
#include "../profiles/IProfile.h"
#include "../mqtt/mqtt_client.h"
#include <functional>

namespace idryer {

/**
 * @brief Top-level runtime coordinator for iDryer devices.
 *
 * Ties together @c CloudStateMachine, @c ActionDispatcher, @c IProfile,
 * and @c MqttClient into a single @c begin() / @c loop() entry point.
 *
 * Built-in handling (always active):
 *   - @c commands/ping — syncs device clock from the timestamp in the payload
 *
 * All other commands are routed to the product-level @c CommandHandler
 * registered via @c setCommandHandler(). The handler receives the command
 * name (suffix after @c commands/) and the JSON payload.
 *
 * When no @c CommandHandler is registered, built-in default routing applies:
 *   - @c commands/invoke  → @c ActionDispatcher
 *   - @c commands/set     → @c ActionDispatcher
 *   - @c invoke device.getConfig → calls @c profile.getConfig() and publishes
 *
 * Typical usage — single handler for both MQTT and local WS:
 * @code
 * IdryerRuntime runtime(&cloud, &dispatcher, &profile, &mqtt);
 *
 * // in setup() — wire the same handler that LocalAccess uses:
 * runtime.setCommandHandler(handleCommand);
 * runtime.begin();
 *
 * // in loop():
 * runtime.loop();
 * @endcode
 */
class IdryerRuntime {
public:
    IdryerRuntime(cloud::CloudStateMachine* cloud,
                  ActionDispatcher*         dispatcher,
                  IProfile*                 profile,
                  MqttClient*               mqtt);

    /**
     * @brief Starts the runtime.
     *
     * Registers the MQTT command callback and calls @c cloud->begin().
     * Call once in @c setup(), after @c setCommandHandler().
     */
    void begin();

    /**
     * @brief Drives the runtime. Call every iteration of @c loop().
     *
     * Calls @c cloud->loop() and @c profile->loop(). Also publishes the
     * info topic once when the device first comes online.
     */
    void loop();

    /// @brief Returns @c true when the cloud state machine is in the @c Online state.
    bool isOnline() const;

    /**
     * @brief Registers the product-level command handler.
     *
     * When set, all incoming MQTT commands (after @c ping is handled internally)
     * are forwarded here. Wire the same function that @c LocalAccess uses as its
     * @c CommandSink — this makes MQTT and local WS converge on one entry point.
     *
     * Signature: @code void handler(const char* command, JsonObjectConst data) @endcode
     *
     * @c command — suffix after @c commands/ (e.g. @c "invoke", @c "set", @c "get_config").
     * @c data    — parsed JSON payload.
     */
    // fnptr + ctx (без std::function).
    using CommandHandler = void (*)(void* ctx, const char* command, JsonObjectConst data);
    void setCommandHandler(CommandHandler handler, void* ctx);

private:
    void onMqttCommand(const char* command, JsonObjectConst data);
    static void onMqttCommandThunk(void* ctx, const char* command, JsonObjectConst data);

    cloud::CloudStateMachine* cloud_;
    ActionDispatcher*         dispatcher_;
    IProfile*                 profile_;
    MqttClient*               mqtt_;

    CommandHandler commandHandler_    = nullptr;
    void*          commandHandlerCtx_ = nullptr;
    bool wasOnline_ = false;
};

} // namespace idryer
