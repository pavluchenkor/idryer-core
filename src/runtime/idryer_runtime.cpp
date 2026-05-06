#if defined(ESP32) || defined(ESP_PLATFORM)

#include "idryer_runtime.h"
#include "../hal/hal_types.h"
#include <string.h>
#include <sys/time.h>

namespace idryer {

IdryerRuntime::IdryerRuntime(cloud::CloudStateMachine* cloud,
                             ActionDispatcher*         dispatcher,
                             IProfile*                 profile,
                             MqttClient*               mqtt)
    : cloud_(cloud), dispatcher_(dispatcher), profile_(profile), mqtt_(mqtt)
{}

void IdryerRuntime::begin() {
    mqtt_->setCommandCallback([this](const char* command, JsonObjectConst data) {
        this->onMqttCommand(command, data);
    });
    cloud_->begin();
}

void IdryerRuntime::loop() {
    cloud_->loop();
    if (profile_) profile_->loop();

    bool online = cloud_->isOnline();
    if (online && !wasOnline_) {
        wasOnline_ = true;
        HAL_LOG_INFO("RT", "Cloud Online");
        if (profile_) {
            profile_->onOnline();
            char infoBuf[512];
            profile_->buildInfoJson(infoBuf, sizeof(infoBuf));
            mqtt_->publishInfoJson(infoBuf);
        }
    }
    if (!online && wasOnline_) {
        wasOnline_ = false;
    }
}

bool IdryerRuntime::isOnline() const { return cloud_->isOnline(); }

void IdryerRuntime::onMqttCommand(const char* command, JsonObjectConst data) {
    HAL_LOG_INFO("RT", "command: %s", command);

    // Time-sync convention: portal вкладывает ISO 8601 `timestamp` в любую
    // команду (см. mqtt_contract.yaml rules.timestamp_convention). Парсим до
    // роутинга — это покрывает все portal-команды, но НЕ local-WS (тот идёт
    // через local_access::setCommandSink, минуя этот метод).
    const char* ts = data["timestamp"].as<const char*>();
    if (ts) {
        struct tm t = {};
        int y, mo, d, h, mi, s;
        if (sscanf(ts, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
            t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
            t.tm_hour = h;        t.tm_min  = mi;     t.tm_sec  = s;
            t.tm_isdst = 0;
            struct timeval tv = { .tv_sec = mktime(&t), .tv_usec = 0 };
            settimeofday(&tv, nullptr);
        }
    }

    // ping: time-sync only (already done above). Info is retained — not
    // republished on every ping (was a spam loop when payload too big).
    if (strcmp(command, "ping") == 0) {
        return;
    }

    // If the product registers a command handler, route everything else through it.
    // This allows MQTT and local WS to share a single business-level handler
    // without duplicating routing logic.
    if (commandHandler_) {
        commandHandler_(command, data);
        return;
    }

    // Default built-in routing — used when no commandHandler is registered.
    // Preserved for backward compatibility with products that rely on the
    // built-in ActionDispatcher wiring.
    if (strcmp(command, "invoke") == 0) {
        const char* action = data["action"].as<const char*>();
        if (action && strcmp(action, "device.getConfig") == 0) {
            if (profile_) {
                DynamicJsonDocument doc(512);
                profile_->getConfig(doc);
                mqtt_->publishConfig(doc);
            }
            return;
        }
        dispatcher_->handleInvoke(data);
        return;
    }

    if (strcmp(command, "set") == 0) {
        dispatcher_->handleSet(data);
        return;
    }

    HAL_LOG_DEBUG("RT", "unhandled command: %s", command);
}

void IdryerRuntime::setCommandHandler(CommandHandler handler) {
    commandHandler_ = std::move(handler);
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
