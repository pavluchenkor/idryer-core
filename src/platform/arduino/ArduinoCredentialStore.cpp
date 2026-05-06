#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoCredentialStore.h"
#include "../../hal/hal_types.h"
#include <string.h>
#include <stdio.h>
#include <esp_mac.h>

namespace idryer {

bool ArduinoCredentialStore::begin() { return true; }

bool ArduinoCredentialStore::load(DeviceIdentity& identity) {
    identity.clear();

    if (!prefs_.begin(kNamespace, true)) {
        HAL_LOG_ERROR("STORE", "Failed to open NVS for reading");
        return false;
    }

    String token    = prefs_.getString("token",    "");
    String deviceId = prefs_.getString("deviceId", "");
    String serial   = prefs_.getString("serial",   "");

    if (token.length()    > 0) identity.setToken(token.c_str());
    if (deviceId.length() > 0) identity.setDeviceId(deviceId.c_str());
    if (serial.length()   > 0) identity.setSerialNumber(serial.c_str());

    prefs_.end();

    HAL_LOG_DEBUG("STORE", "Loaded: serial=%s token=%s deviceId=%s",
                  identity.hasSerialNumber() ? identity.serialNumber : "(none)",
                  identity.hasToken()        ? "yes" : "no",
                  identity.hasDeviceId()     ? identity.deviceId : "(none)");

    return identity.hasToken();
}

bool ArduinoCredentialStore::save(const DeviceIdentity& identity) {
    if (!prefs_.begin(kNamespace, false)) {
        HAL_LOG_ERROR("STORE", "Failed to open NVS for writing");
        return false;
    }
    prefs_.putString("token",    identity.token);
    prefs_.putString("deviceId", identity.deviceId);
    prefs_.putString("serial",   identity.serialNumber);
    prefs_.end();
    HAL_LOG_INFO("STORE", "Saved credentials to NVS");
    return true;
}

void ArduinoCredentialStore::clear() {
    if (!prefs_.begin(kNamespace, false)) {
        HAL_LOG_ERROR("STORE", "Failed to open NVS for clearing");
        return;
    }
    prefs_.clear();
    prefs_.end();
    HAL_LOG_INFO("STORE", "Cleared all credentials from NVS");
}

void ArduinoCredentialStore::seedSerialFromMac() {
    prefs_.begin(kNamespace, true);
    String existing = prefs_.getString("serial", "");
    prefs_.end();

    if (existing.length() > 0) {
        HAL_LOG_INFO("STORE", "Serial from NVS: %s", existing.c_str());
        return;
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char serial[24];
    snprintf(serial, sizeof(serial), "DEVICE_%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    prefs_.begin(kNamespace, false);
    prefs_.putString("serial", serial);
    prefs_.end();
    HAL_LOG_INFO("STORE", "Serial generated from MAC: %s", serial);
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
