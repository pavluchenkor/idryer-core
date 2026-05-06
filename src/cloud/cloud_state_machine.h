#pragma once

#include "../core/types.h"
#include "../core/config.h"
#include "../hal/hal_types.h"
#include "../device/interfaces/IWifiManager.h"
#include "../device/interfaces/ICredentialStore.h"
#include "../mqtt/mqtt_client.h"
#include "http_api.h"

namespace idryer {
namespace cloud {

/**
 * @brief Cloud connectivity states, in order of progression.
 *
 * The state machine advances through these states as the device connects
 * to WiFi, provisions itself with the backend, and establishes MQTT.
 */
enum class CloudState : uint8_t {
    Idle,              ///< Not started.
    WifiConnecting,    ///< Waiting for WiFi to connect.
    WaitingForMcuSerial, ///< Two-MCU only: waiting for Hello from RP2040.
    Provisioning,      ///< Registering the device with the backend API.
    Registering,       ///< Sending device registration request.
    AwaitingClaim,     ///< Device is registered but not yet claimed by a user.
    Ready,             ///< Claimed and has credentials, ready to connect MQTT.
    MqttConnecting,    ///< Attempting MQTT connection.
    Online             ///< Fully connected and online.
};

const char* cloudStateToString(CloudState state);

/// @brief Called when the cloud state changes.
typedef void (*CloudStateChangeCallback)(CloudState oldState, CloudState newState, void* ctx);

/// @brief Called when a claim PIN is available for the user to enter.
typedef void (*ClaimPinCallback)(const char* pin, uint32_t expiresInSeconds, void* ctx);

/// @brief Called when the device has been successfully claimed.
typedef void (*ClaimCompleteCallback)(const char* deviceId, void* ctx);

/// @brief Called when the device reaches @c AwaitingClaim (needs user action).
typedef void (*UnclaimedCallback)(void* ctx);

/**
 * @brief Configuration for retry intervals and device mode.
 *
 * All timeouts are in milliseconds. The defaults work for most devices.
 */
struct CloudConfig {
    uint32_t wifiRetryIntervalMs   = IDRYER_WIFI_RETRY_INTERVAL_MS;
    uint32_t provisionRetryMs      = IDRYER_PROVISION_RETRY_MS;
    uint32_t claimPollIntervalMs   = IDRYER_CLAIM_POLL_INTERVAL_MS;
    uint32_t mqttRetryIntervalMs   = IDRYER_MQTT_RETRY_INTERVAL_MS;
    /// Set to @c true for two-MCU devices (ESP32 + RP2040). Default is @c false.
    bool waitForMcuSerial = false;
};

/**
 * @brief Drives the full cloud lifecycle: WiFi → provisioning → claim → MQTT.
 *
 * Instantiate with your platform implementations and pass to @c IdryerRuntime.
 * You don't call most methods directly — @c IdryerRuntime calls @c begin() and
 * @c loop() for you.
 *
 * The only methods you typically call from product code are:
 *   - @c setStateChangeCallback() — to start NTP, update a status LED, etc.
 *   - @c setUnclaimedCallback()   — to trigger auto-claim on standalone devices.
 */
class CloudStateMachine {
public:
    CloudStateMachine(IWifiManager* wifi,
                      ICredentialStore* store,
                      HttpApi* api,
                      MqttClient* mqtt,
                      const CloudConfig& config = CloudConfig{});

    /// @brief Starts the state machine. Called by @c IdryerRuntime::begin().
    void begin();

    /// @brief Advances the state machine. Called by @c IdryerRuntime::loop().
    void loop();

    CloudState getState() const { return state_; }

    /// @brief Returns @c true when the state is @c Online.
    bool isOnline() const { return state_ == CloudState::Online; }

    /**
     * @brief Initiates a device claim request.
     *
     * On standalone (ESP32-only) devices, call this from the @c UnclaimedCallback.
     * The backend will issue a PIN for the user to enter.
     *
     * @return @c true if the request was sent successfully.
     */
    bool requestClaim();

    /**
     * @brief Passes the serial number received from the RP2040 controller.
     *
     * Only needed for two-MCU devices with @c waitForMcuSerial = @c true.
     * Call this from the UART Hello handler.
     */
    void setMcuSerial(const char* mcuSerial);

    void setWaitForMcuSerial(bool wait) { config_.waitForMcuSerial = wait; }

    /// @brief Forces a token refresh (typically not needed in normal flow).
    bool refreshToken();

    const DeviceIdentity& getIdentity() const { return identity_; }

    /**
     * @brief Registers a callback for state transitions.
     *
     * Use this to react to specific transitions, e.g.:
     * - @c WifiConnecting → next: start NTP sync
     * - @c Online: update a status LED
     *
     * @param cb  Callback function.
     * @param ctx Arbitrary context pointer passed back to the callback.
     */
    void setStateChangeCallback(CloudStateChangeCallback cb, void* ctx);

    /**
     * @brief Registers a callback for when a claim PIN becomes available.
     *
     * The PIN should be displayed to the user so they can enter it in the app.
     */
    void setClaimPinCallback(ClaimPinCallback cb, void* ctx);

    /// @brief Registers a callback for when the device is successfully claimed.
    void setClaimCompleteCallback(ClaimCompleteCallback cb, void* ctx);

    /**
     * @brief Registers a callback for when the device is unclaimed.
     *
     * On standalone devices, use this to auto-trigger @c requestClaim():
     * @code
     * cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
     * @endcode
     */
    void setUnclaimedCallback(UnclaimedCallback cb, void* ctx);

private:
    void handleWifiConnecting();
    void handleWaitingForMcuSerial();
    void handleProvisioning();
    void handleAwaitingClaim();
    void handleReady();
    void handleMqttConnecting();
    void handleOnline();
    void setState(CloudState newState);

    IWifiManager*     wifi_;
    ICredentialStore* store_;
    HttpApi*          api_;
    MqttClient*       mqtt_;
    CloudConfig       config_;

    CloudState     state_    = CloudState::Idle;
    DeviceIdentity identity_;

    uint32_t lastWifiAttempt_      = 0;
    uint32_t lastProvisionAttempt_ = 0;
    uint32_t lastTokenRefreshMs_   = 0;
    uint32_t lastClaimPoll_        = 0;
    uint32_t lastMqttAttempt_      = 0;

    bool awaitingClaim_      = false;
    bool mqttInitialized_    = false;
    bool unclaimedNotified_  = false;
    bool serialVerified_     = false;

    char     pendingPin_[IDRYER_MAX_PIN_LEN];
    uint32_t pinCreatedAtMs_  = 0;
    uint32_t pinTotalSeconds_ = 0;

    CloudStateChangeCallback stateCallback_        = nullptr;
    void*                    stateCallbackCtx_      = nullptr;
    ClaimPinCallback         claimPinCallback_      = nullptr;
    void*                    claimPinCtx_           = nullptr;
    ClaimCompleteCallback    claimCompleteCallback_ = nullptr;
    void*                    claimCompleteCtx_      = nullptr;
    UnclaimedCallback        unclaimedCallback_     = nullptr;
    void*                    unclaimedCtx_          = nullptr;
};

} // namespace cloud
} // namespace idryer
