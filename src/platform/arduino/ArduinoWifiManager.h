#pragma once

#include "../../device/interfaces/IWifiManager.h"
#include "../../core/config.h"
#include <WiFi.h>

namespace idryer {

/**
 * @brief WiFi connection manager for ESP32 (Arduino framework).
 *
 * Stores the SSID and password, then drives the WiFi connection in @c loop().
 * Reconnects automatically if the connection drops.
 *
 * Used by @c CloudStateMachine — you don't need to call @c connect() or
 * @c loop() directly; @c CloudStateMachine does it for you.
 *
 * You do need to call @c begin() with the credentials before @c runtime.begin().
 */
class ArduinoWifiManager : public IWifiManager {
public:
    ArduinoWifiManager();

    /**
     * @brief Stores credentials and initiates a connection attempt.
     *
     * Safe to call multiple times (e.g. after receiving new Improv credentials).
     *
     * @param ssid     WiFi network name.
     * @param password WiFi password. Pass @c "" for open networks.
     */
    void begin(const char* ssid, const char* password) override;

    /// @brief Attempts a connection if not already connected. Returns @c true if connected.
    bool connect() override;

    /// @brief Returns @c true if currently connected to WiFi.
    bool isConnected() override;

    /// @brief Disconnects from WiFi.
    void disconnect() override;

    /// @brief Writes the current local IP address into @p buffer (e.g. @c "192.168.1.10").
    void getLocalIP(char* buffer, size_t bufferSize) override;

    /// @brief Writes the connected SSID into @p buffer.
    void getSSID(char* buffer, size_t bufferSize) override;

    /// @brief Returns the current WiFi signal strength in dBm.
    int getRSSI() override;

    /// @brief Writes the WiFi MAC address into @p buffer (e.g. @c "AA:BB:CC:DD:EE:FF").
    void getMacAddress(char* buffer, size_t bufferSize) override;

    /// @brief Must be called from the main loop. Handles reconnection.
    void loop() override;

private:
    char ssid_[IDRYER_MAX_SSID_LEN];
    char password_[IDRYER_MAX_PASSWORD_LEN];
    bool scanLogged_ = false;
};

} // namespace idryer
