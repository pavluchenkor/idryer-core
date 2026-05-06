#pragma once

#include "config.h"
#include <string.h>

namespace idryer {

struct DeviceIdentity {
    char serialNumber[IDRYER_MAX_SERIAL_NUMBER_LEN];
    char token[IDRYER_MAX_TOKEN_LEN];
    char deviceId[IDRYER_MAX_DEVICE_ID_LEN];

    DeviceIdentity() { clear(); }

    void clear() {
        memset(serialNumber, 0, sizeof(serialNumber));
        memset(token, 0, sizeof(token));
        memset(deviceId, 0, sizeof(deviceId));
    }

    bool hasToken() const         { return token[0] != '\0'; }
    bool hasDeviceId() const      { return deviceId[0] != '\0'; }
    bool hasSerialNumber() const  { return serialNumber[0] != '\0'; }

    void setSerialNumber(const char* src) {
        if (src) { strncpy(serialNumber, src, sizeof(serialNumber) - 1); serialNumber[sizeof(serialNumber)-1] = '\0'; }
    }
    void setToken(const char* src) {
        if (src) { strncpy(token, src, sizeof(token) - 1); token[sizeof(token)-1] = '\0'; }
    }
    void setDeviceId(const char* src) {
        if (src) { strncpy(deviceId, src, sizeof(deviceId) - 1); deviceId[sizeof(deviceId)-1] = '\0'; }
    }
};

struct WifiCredentials {
    char ssid[IDRYER_MAX_SSID_LEN];
    char password[IDRYER_MAX_PASSWORD_LEN];

    WifiCredentials() {
        memset(ssid, 0, sizeof(ssid));
        memset(password, 0, sizeof(password));
    }

    void setSsid(const char* src) {
        if (src) { strncpy(ssid, src, sizeof(ssid) - 1); ssid[sizeof(ssid)-1] = '\0'; }
    }
    void setPassword(const char* src) {
        if (src) { strncpy(password, src, sizeof(password) - 1); password[sizeof(password)-1] = '\0'; }
    }
};

typedef void (*TimeoutCallback)(void* context);
typedef void (*CommandCallback)(const char* command, const char* jsonData, void* context);
typedef void (*StateChangeCallback)(uint8_t oldState, uint8_t newState, void* context);

inline size_t safeCopy(char* dest, size_t destSize, const char* src) {
    if (!dest || destSize == 0) return 0;
    if (!src) { dest[0] = '\0'; return 0; }
    size_t i = 0;
    while (i < destSize - 1 && src[i] != '\0') { dest[i] = src[i]; i++; }
    dest[i] = '\0';
    return i;
}

inline bool isNotEmpty(const char* str) {
    return str != nullptr && str[0] != '\0';
}

} // namespace idryer
