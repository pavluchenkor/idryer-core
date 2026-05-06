#pragma once

#include <ArduinoJson.h>

namespace idryer {

/**
 * @brief Routes @c commands/invoke and @c commands/set MQTT messages to product handlers.
 *
 * Register one @c InvokeHandler and one @c SetCallback in your product assembly.
 * @c IdryerRuntime calls @c handleInvoke() / @c handleSet() automatically when
 * the corresponding MQTT messages arrive.
 *
 * Uses plain function pointers (not @c std::function) to stay lightweight and
 * avoid heap allocation.
 *
 * Example — wiring in @c setup():
 * @code
 * dispatcher.setInvokeHandler(
 *     [](const char* action, JsonObjectConst args, void* ctx) -> bool {
 *         return static_cast<MyDevice*>(ctx)->executor.execute(action, args);
 *     }, this);
 *
 * dispatcher.setSetCallback(
 *     [](JsonObjectConst data, void* ctx) {
 *         int id  = data["id"]  | -1;
 *         int val = data["val"] | -1;
 *         static_cast<MyDevice*>(ctx)->profile.applyConfig(id, val);
 *     }, this);
 * @endcode
 */
class ActionDispatcher {
public:
    /**
     * @brief Handler for @c commands/invoke.
     * @param action The action name from the @c "action" field (e.g. @c "led.pulse").
     * @param args   The @c "args" object from the payload.
     * @param ctx    Context pointer passed to @c setInvokeHandler().
     * @return @c true if the action was handled, @c false if unrecognized.
     */
    using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

    /**
     * @brief Callback for @c commands/set.
     * @param data The full JSON payload (contains @c "id" and @c "val").
     * @param ctx  Context pointer passed to @c setSetCallback().
     */
    using SetCallback = void (*)(JsonObjectConst data, void* ctx);

    /// @brief Registers the invoke handler. Pass @c nullptr as @p ctx if not needed.
    void setInvokeHandler(InvokeHandler fn, void* ctx) { invokeHandler_ = fn; invokeHandlerCtx_ = ctx; }

    /// @brief Registers the set callback. Pass @c nullptr as @p ctx if not needed.
    void setSetCallback(SetCallback fn, void* ctx)     { setCallback_ = fn; setCallbackCtx_ = ctx; }

    /// @brief Called by @c IdryerRuntime when a @c commands/invoke message arrives.
    void handleInvoke(JsonObjectConst data);

    /// @brief Called by @c IdryerRuntime when a @c commands/set message arrives.
    void handleSet(JsonObjectConst data);

private:
    InvokeHandler invokeHandler_    = nullptr;
    void*         invokeHandlerCtx_ = nullptr;
    SetCallback   setCallback_      = nullptr;
    void*         setCallbackCtx_   = nullptr;
};

} // namespace idryer
