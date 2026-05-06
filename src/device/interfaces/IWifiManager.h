#pragma once

#include "../../core/config.h"
#include <stdint.h>

namespace idryer {

class IWifiManager {
public:
    virtual ~IWifiManager() = default;

    virtual void begin(const char* ssid, const char* password) = 0;
    virtual bool connect() = 0;
    virtual bool isConnected() = 0;
    virtual void disconnect() = 0;
    virtual void getLocalIP(char* buffer, size_t bufferSize) = 0;
    virtual void getSSID(char* buffer, size_t bufferSize) = 0;
    virtual int getRSSI() = 0;
    virtual void getMacAddress(char* buffer, size_t bufferSize) = 0;
    virtual void loop() = 0;
};

} // namespace idryer
