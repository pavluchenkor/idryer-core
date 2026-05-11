#pragma once

#include <ArduinoJson.h>

namespace idryer {

/**
 * Calls your functions when "invoke" or "set" MQTT commands arrive.
 *
 * In setup(): store your functions with setInvokeHandler() / setSetCallback().
 * IdryerRuntime then calls handleInvoke() / handleSet() automatically on each command.
 * Uses plain function pointers — no heap allocation.
 *
 * Example:
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
     * Function pointer type for "invoke" commands.
     * @param action  Action name from the payload, e.g. "led.pulse".
     * @param args    The "args" object from the payload.
     * @param ctx     Your context pointer (passed to setInvokeHandler).
     * @return true if the action was handled, false if unknown.
     */
    using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

    /**
     * Function pointer type for "set" commands.
     * @param data  Full JSON payload — contains "id" and "val".
     * @param ctx   Your context pointer (passed to setSetCallback).
     */
    using SetCallback = void (*)(JsonObjectConst data, void* ctx);

    /// Stores fn as the function to call on "invoke" commands. Pass nullptr for ctx if not needed.
    void setInvokeHandler(InvokeHandler fn, void* ctx) { invokeHandler_ = fn; invokeHandlerCtx_ = ctx; }

    /// Stores fn as the function to call on "set" commands. Pass nullptr for ctx if not needed.
    void setSetCallback(SetCallback fn, void* ctx)     { setCallback_ = fn; setCallbackCtx_ = ctx; }

    /// Reads action and args from data, then calls the stored invoke handler.
    void handleInvoke(JsonObjectConst data);

    /// Passes data to the stored set callback.
    void handleSet(JsonObjectConst data);

private:
    InvokeHandler invokeHandler_    = nullptr;
    void*         invokeHandlerCtx_ = nullptr;
    SetCallback   setCallback_      = nullptr;
    void*         setCallbackCtx_   = nullptr;
};

} // namespace idryer
