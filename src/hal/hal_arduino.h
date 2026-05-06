#pragma once

#include "hal_types.h"
#include <Arduino.h>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include "driver/uart.h"
#endif

namespace idryer {
namespace hal {

class ArduinoTime : public ITime {
public:
    uint32_t millis() override  { return ::millis(); }
    uint32_t micros() override  { return ::micros(); }
    void delayMs(uint32_t ms) override { ::delay(ms); }
    void delayUs(uint32_t us) override { ::delayMicroseconds(us); }
};

class ArduinoLogger : public ILogger {
public:
    explicit ArduinoLogger(Stream& output, bool enableColors = true)
        : output_(output), level_(LogLevel::Debug), enableColors_(enableColors) {}

    void log(LogLevel level, const char* tag, const char* format, ...) override;
    void setLevel(LogLevel level) override { level_ = level; }
    LogLevel getLevel() const override { return level_; }
    void setColors(bool enable) { enableColors_ = enable; }

private:
    Stream& output_;
    LogLevel level_;
    bool enableColors_;

    const char* levelToString(LogLevel level) const;
    const char* levelToColor(LogLevel level) const;
};

/**
 * @brief Initializes the HAL logging backend.
 *
 * Must be called in @c setup() before any @c HAL_LOG_* macros are used.
 *
 * Pass @c nullptr as @p debugStream to suppress all log output (e.g. during
 * Improv provisioning when Serial belongs to Improv). Call again with
 * @c &Serial once WiFi connects and Serial is free.
 *
 * @code
 * // In setup() — suppress logs while Improv owns Serial:
 * idryer::hal::initArduinoHal(nullptr);
 *
 * // Once WiFi connects — enable logs:
 * idryer::hal::initArduinoHal(&Serial);
 * @endcode
 *
 * @param debugStream  Output stream for log messages (@c &Serial, @c &Serial1, etc.),
 *                     or @c nullptr to discard all logs.
 * @param enableColors Whether to emit ANSI color codes. Default: @c true.
 */
void initArduinoHal(Stream* debugStream = nullptr, bool enableColors = true);

/// @brief Shuts down the HAL and releases the logger. Rarely needed.
void deinitArduinoHal();

/**
 * @brief Wraps a @c HardwareSerial as an @c ISerial for use with @c UartBridge.
 *
 * @code
 * ArduinoSerial bridgeSerial(Serial1, 1);  // ESP32 UART1
 * bridge.begin(&bridgeSerial, 115200);
 * @endcode
 *
 * @note The @p uartNum parameter is used to call @c uart_wait_tx_done() before
 *       reads, ensuring the TX buffer is flushed. Pass @c -1 if not needed.
 */
class ArduinoSerial : public ISerial {
public:
    /**
     * @param serial   The Arduino hardware serial port.
     * @param uartNum  ESP32 UART number (0, 1, or 2) for TX-done flush. Pass @c -1 to skip.
     */
    explicit ArduinoSerial(HardwareSerial& serial, int uartNum = -1)
        : serial_(serial)
#if defined(ESP32) || defined(ESP_PLATFORM)
        , uartNum_(uartNum)
#endif
    {}

    void begin(uint32_t baudRate) override { serial_.begin(baudRate); }
    int available() override              { return serial_.available(); }
    int read() override                   { return serial_.read(); }
    size_t readBytes(uint8_t* buf, size_t len) override { return serial_.readBytes(buf, len); }
    size_t write(const uint8_t* buf, size_t len) override { return serial_.write(buf, len); }

    void flush() override {
        serial_.flush();
#if defined(ESP32) || defined(ESP_PLATFORM)
        if (uartNum_ >= 0 && uartNum_ < UART_NUM_MAX) {
            uart_wait_tx_done((uart_port_t)uartNum_, portMAX_DELAY);
        }
#endif
    }

private:
    HardwareSerial& serial_;
#if defined(ESP32) || defined(ESP_PLATFORM)
    int uartNum_;
#endif
};

} // namespace hal
} // namespace idryer
