#pragma once

#include <Preferences.h>

namespace idryer {

/**
 * @brief Saves and loads WiFi credentials (SSID + password) in NVS.
 *
 * Uses the NVS namespace @c "wifi". Reusable across all ESP32 products
 * that use Improv or any other provisioning flow.
 *
 * Typical usage:
 * @code
 * ArduinoWifiStore wifiStore;
 *
 * // On first boot or after Improv provisioning:
 * wifiStore.save(ssid, password);
 *
 * // On every boot — restore saved credentials:
 * char ssid[64], pass[64];
 * if (wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
 *     wifi.begin(ssid, pass);
 * }
 * @endcode
 */
class ArduinoWifiStore {
public:
    /**
     * @brief Loads saved WiFi credentials from NVS.
     * @param ssid      Buffer to write the SSID into.
     * @param ssidLen   Size of the SSID buffer (at least 33 bytes recommended).
     * @param password  Buffer to write the password into.
     * @param passLen   Size of the password buffer (at least 64 bytes recommended).
     * @return @c true if a non-empty SSID was found, @c false if NVS has no credentials.
     */
    bool load(char* ssid, size_t ssidLen, char* password, size_t passLen);

    /**
     * @brief Saves WiFi credentials to NVS.
     *
     * Overwrites any previously stored credentials. Passing @c nullptr is safe
     * and stores an empty string.
     *
     * @param ssid     The WiFi network name.
     * @param password The WiFi password. Pass @c "" for open networks.
     */
    void save(const char* ssid, const char* password);

private:
    Preferences prefs_;
};

} // namespace idryer
