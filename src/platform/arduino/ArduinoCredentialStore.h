#pragma once

#include "../../device/interfaces/ICredentialStore.h"
#include <Preferences.h>

namespace idryer {

/**
 * @brief Stores and loads cloud device identity in NVS.
 *
 * Persists three fields in the @c "idryer" NVS namespace:
 *   - @c serial   — device serial number (used as MQTT username)
 *   - @c token    — device token (used as MQTT password)
 *   - @c deviceId — backend device ID (UUID, assigned after claim)
 *
 * Also provides @c seedSerialFromMac() to auto-generate a serial from the
 * WiFi MAC address if one hasn't been assigned yet.
 */
class ArduinoCredentialStore : public ICredentialStore {
public:
    /// @brief No-op. Always returns @c true.
    bool begin() override;

    /**
     * @brief Reads credentials from NVS into @p identity.
     * @return @c true if a non-empty token was found (device is provisioned).
     *         @c false if no token exists (device needs provisioning).
     */
    bool load(DeviceIdentity& identity) override;

    /**
     * @brief Writes credentials to NVS.
     * @return @c true on success.
     */
    bool save(const DeviceIdentity& identity) override;

    /// @brief Erases all fields in the @c "idryer" NVS namespace.
    void clear() override;

    /**
     * @brief Ensures the device has a serial number.
     *
     * If no serial is stored in NVS, generates one from the WiFi MAC address
     * in the format @c DEVICE_AABBCCDDEEFF and saves it. Does nothing if a
     * serial already exists.
     *
     * Call this in @c setup() before @c runtime.begin().
     */
    void seedSerialFromMac();

private:
    static constexpr const char* kNamespace = "idryer";
    Preferences prefs_;
};

} // namespace idryer
