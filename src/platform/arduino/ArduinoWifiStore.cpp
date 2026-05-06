#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoWifiStore.h"
#include "../../hal/hal_types.h"

namespace idryer {

bool ArduinoWifiStore::load(char* ssid, size_t ssidLen, char* password, size_t passLen) {
    prefs_.begin("wifi", true);
    size_t sLen = prefs_.getString("ssid", ssid, ssidLen);
    prefs_.getString("password", password, passLen);
    prefs_.end();
    if (sLen == 0) { ssid[0] = '\0'; password[0] = '\0'; return false; }
    return true;
}

void ArduinoWifiStore::save(const char* ssid, const char* password) {
    prefs_.begin("wifi", false);
    prefs_.putString("ssid",     ssid     ? ssid     : "");
    prefs_.putString("password", password ? password : "");
    prefs_.end();
    HAL_LOG_INFO("WSTORE", "WiFi credentials saved");
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
