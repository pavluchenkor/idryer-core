#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

namespace idryer {
namespace hal {

class ITime {
public:
    virtual ~ITime() = default;
    virtual uint32_t millis() = 0;
    virtual uint32_t micros() = 0;
    virtual void delayMs(uint32_t ms) = 0;
    virtual void delayUs(uint32_t us) = 0;
};

class ISerial {
public:
    virtual ~ISerial() = default;
    virtual void begin(uint32_t baudRate) = 0;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t readBytes(uint8_t* buffer, size_t length) = 0;
    virtual size_t write(const uint8_t* buffer, size_t length) = 0;
    virtual void flush() = 0;
};

enum class LogLevel : uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    None = 4
};

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, const char* tag, const char* format, ...) = 0;
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
};

struct HalContext {
    ITime*   time;
    ILogger* logger;
};

extern HalContext* g_hal;

#define HAL_MILLIS()       (idryer::hal::g_hal->time->millis())
#define HAL_MICROS()       (idryer::hal::g_hal->time->micros())
#define HAL_DELAY_MS(ms)   (idryer::hal::g_hal->time->delayMs(ms))
#define HAL_DELAY_US(us)   (idryer::hal::g_hal->time->delayUs(us))

#ifndef HAL_NO_LOG
#define HAL_LOG(level, tag, fmt, ...) \
    do { if (idryer::hal::g_hal && idryer::hal::g_hal->logger) \
        idryer::hal::g_hal->logger->log(level, tag, fmt, ##__VA_ARGS__); } while(0)
#define HAL_LOG_DEBUG(tag, fmt, ...) HAL_LOG(idryer::hal::LogLevel::Debug,   tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_INFO(tag, fmt, ...)  HAL_LOG(idryer::hal::LogLevel::Info,    tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_WARN(tag, fmt, ...)  HAL_LOG(idryer::hal::LogLevel::Warning, tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_ERROR(tag, fmt, ...) HAL_LOG(idryer::hal::LogLevel::Error,   tag, fmt, ##__VA_ARGS__)
#else
#define HAL_LOG(level, tag, fmt, ...)    do {} while(0)
#define HAL_LOG_DEBUG(tag, fmt, ...)     do {} while(0)
#define HAL_LOG_INFO(tag, fmt, ...)      do {} while(0)
#define HAL_LOG_WARN(tag, fmt, ...)      do {} while(0)
#define HAL_LOG_ERROR(tag, fmt, ...)     do {} while(0)
#endif

} // namespace hal
} // namespace idryer
