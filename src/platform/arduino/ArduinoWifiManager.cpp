#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ArduinoWifiManager.h"
#include "../../hal/hal_types.h"
#include <esp_wifi.h>
#include <string.h>

namespace idryer {

ArduinoWifiManager::ArduinoWifiManager() {
    memset(ssid_, 0, sizeof(ssid_));
    memset(password_, 0, sizeof(password_));
}

void ArduinoWifiManager::begin(const char* ssid, const char* password) {
    if (ssid)     { strncpy(ssid_,     ssid,     sizeof(ssid_) - 1);     ssid_[sizeof(ssid_)-1]         = '\0'; }
    if (password) { strncpy(password_, password, sizeof(password_) - 1); password_[sizeof(password_)-1] = '\0'; }

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    esp_wifi_set_ps(WIFI_PS_NONE);

    HAL_LOG_INFO("WIFI", "Initialized with SSID: %s", ssid_);
}

bool ArduinoWifiManager::connect() {
    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) return true;

    if (!scanLogged_) {
        HAL_LOG_INFO("WIFI", "Scanning networks...");
        const int n = WiFi.scanNetworks(false, true);
        for (int i = 0; i < n; ++i)
            HAL_LOG_DEBUG("WIFI", "AP %d: %s (RSSI=%d dBm)", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        WiFi.scanDelete();
        scanLogged_ = true;
        HAL_LOG_INFO("WIFI", "Connecting to SSID: %s", ssid_);
        WiFi.begin(ssid_, password_);
    } else {
        HAL_LOG_DEBUG("WIFI", "Status: %d, reconnecting...", status);
        WiFi.begin(ssid_, password_);
    }
    return false;
}

bool ArduinoWifiManager::isConnected() { return WiFi.status() == WL_CONNECTED; }

void ArduinoWifiManager::disconnect() { WiFi.disconnect(); scanLogged_ = false; }

void ArduinoWifiManager::getLocalIP(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    String ip = WiFi.localIP().toString();
    strncpy(buffer, ip.c_str(), bufferSize - 1);
    buffer[bufferSize-1] = '\0';
}

void ArduinoWifiManager::getSSID(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    if (isConnected()) {
        String ssid = WiFi.SSID();
        strncpy(buffer, ssid.c_str(), bufferSize - 1);
        buffer[bufferSize-1] = '\0';
    } else {
        buffer[0] = '\0';
    }
}

int ArduinoWifiManager::getRSSI() { return WiFi.RSSI(); }

void ArduinoWifiManager::getMacAddress(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    String mac = WiFi.macAddress();
    strncpy(buffer, mac.c_str(), bufferSize - 1);
    buffer[bufferSize-1] = '\0';
}

void ArduinoWifiManager::loop() {}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
