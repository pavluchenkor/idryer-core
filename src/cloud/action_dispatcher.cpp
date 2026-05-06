#if defined(ESP32) || defined(ESP_PLATFORM)

#include "action_dispatcher.h"
#include "../hal/hal_types.h"
#include <string.h>

namespace idryer {

void ActionDispatcher::handleInvoke(JsonObjectConst data) {
    const char* action = data["action"].as<const char*>();
    if (!action || action[0] == '\0') {
        HAL_LOG_WARN("DISP", "invoke: missing 'action' field");
        return;
    }
    HAL_LOG_INFO("DISP", "invoke: action=%s", action);

    JsonObjectConst args = data["args"].as<JsonObjectConst>();
    if (invokeHandler_ && invokeHandler_(action, args, invokeHandlerCtx_)) return;

    HAL_LOG_WARN("DISP", "invoke: unhandled action=%s", action);
}

void ActionDispatcher::handleSet(JsonObjectConst data) {
    if (setCallback_) setCallback_(data, setCallbackCtx_);
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
