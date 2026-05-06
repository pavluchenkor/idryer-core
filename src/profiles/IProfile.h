#pragma once

#include <ArduinoJson.h>

namespace idryer {

/**
 * @brief Interface that a device profile must implement.
 *
 * A profile encapsulates the device-specific behavior: what config parameters
 * the device has, what its info payload looks like, and how it runs over time.
 *
 * @c IdryerRuntime calls these methods at the right moments — you don't call
 * them directly from your main loop.
 *
 * To create a device profile, subclass this and implement all five methods.
 * See @c LedStripProfile for a concrete example.
 */
class IProfile {
public:
    virtual ~IProfile() = default;

    /**
     * @brief Called once when the device comes online (cloud state reaches Online).
     *
     * Use this to load config from NVS and apply it to hardware.
     */
    virtual void onOnline() = 0;

    /**
     * @brief Called every iteration of the main loop.
     *
     * Handle time-based tasks here (e.g. LED timers, sensor polling).
     */
    virtual void loop() = 0;

    /**
     * @brief Fills @p out with the current device config as JSON.
     *
     * Called by @c IdryerRuntime when the backend sends a @c device.getConfig invoke.
     * The result is published to @c idryer/{serial}/config.
     *
     * @param out Empty @c JsonDocument to fill in.
     */
    virtual void getConfig(JsonDocument& out) = 0;

    /**
     * @brief Applies a single config parameter received from @c commands/set.
     *
     * The backend sends @c {"id": N, "val": V}. Map the @p id to a hardware
     * parameter in your implementation.
     *
     * @param id  Config parameter ID (product-defined).
     * @param val New value.
     * @return @c true if the parameter was recognized and applied, @c false otherwise.
     */
    virtual bool applyConfig(int id, int val) = 0;

    /**
     * @brief Serializes the device info JSON into @p buf.
     *
     * Called by @c IdryerRuntime when the device comes online.
     * The result is published (retained) to @c idryer/{serial}/info.
     *
     * @param buf Buffer to write into.
     * @param len Size of the buffer.
     */
    virtual void buildInfoJson(char* buf, size_t len) const = 0;
};

} // namespace idryer
