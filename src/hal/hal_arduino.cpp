#include "hal_arduino.h"
#include <stdio.h>

namespace idryer {
namespace hal {

HalContext* g_hal = nullptr;

static ArduinoTime    s_time;
static ArduinoLogger* s_logger = nullptr;
static HalContext     s_context;

void ArduinoLogger::log(LogLevel level, const char* tag, const char* format, ...) {
    if (level < level_) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (enableColors_) {
        output_.printf("%s[%s] %s: %s\033[0m\n",
                       levelToColor(level), levelToString(level),
                       tag ? tag : "???", buffer);
    } else {
        output_.printf("[%s] %s: %s\n",
                       levelToString(level), tag ? tag : "???", buffer);
    }
}

const char* ArduinoLogger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        default:                return "?????";
    }
}

const char* ArduinoLogger::levelToColor(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "\033[90m";
        case LogLevel::Info:    return "\033[32m";
        case LogLevel::Warning: return "\033[33m";
        case LogLevel::Error:   return "\033[31m";
        default:                return "\033[0m";
    }
}

void initArduinoHal(Stream* debugStream, bool enableColors) {
    s_context.time = &s_time;

    if (debugStream != nullptr) {
        if (s_logger != nullptr) delete s_logger;
        s_logger = new ArduinoLogger(*debugStream, enableColors);
        s_context.logger = s_logger;
    } else {
        s_context.logger = nullptr;
    }

    g_hal = &s_context;
}

void deinitArduinoHal() {
    if (s_logger != nullptr) { delete s_logger; s_logger = nullptr; }
    s_context.logger = nullptr;
    g_hal = nullptr;
}

} // namespace hal
} // namespace idryer
