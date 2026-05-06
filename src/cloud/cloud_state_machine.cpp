#if defined(ESP32) || defined(ESP_PLATFORM)

#include "cloud_state_machine.h"
#include <stdio.h>
#include <stdlib.h>

namespace idryer {
namespace cloud {

const char* cloudStateToString(CloudState state) {
    switch (state) {
        case CloudState::Idle:                return "Idle";
        case CloudState::WifiConnecting:      return "WifiConnecting";
        case CloudState::WaitingForMcuSerial: return "WaitingForMcuSerial";
        case CloudState::Provisioning:        return "Provisioning";
        case CloudState::Registering:         return "Registering";
        case CloudState::AwaitingClaim:       return "AwaitingClaim";
        case CloudState::Ready:               return "Ready";
        case CloudState::MqttConnecting:      return "MqttConnecting";
        case CloudState::Online:              return "Online";
        default:                              return "Unknown";
    }
}

CloudStateMachine::CloudStateMachine(IWifiManager* wifi, ICredentialStore* store,
                                     HttpApi* api, MqttClient* mqtt,
                                     const CloudConfig& config)
    : wifi_(wifi), store_(store), api_(api), mqtt_(mqtt), config_(config)
{
    pendingPin_[0] = '\0';
}

void CloudStateMachine::begin() {
    store_->begin();
    store_->load(identity_);

    HAL_LOG_INFO("CLOUD", "Init: serial=%s deviceId=%s",
                 identity_.hasSerialNumber() ? identity_.serialNumber : "(waiting)",
                 identity_.hasDeviceId()     ? identity_.deviceId     : "(none)");

    setState(CloudState::WifiConnecting);
    lastWifiAttempt_ = HAL_MILLIS() - config_.wifiRetryIntervalMs;
}

void CloudStateMachine::loop() {
    wifi_->loop();

    switch (state_) {
        case CloudState::WifiConnecting:      handleWifiConnecting();      break;
        case CloudState::WaitingForMcuSerial: handleWaitingForMcuSerial(); break;
        case CloudState::Provisioning:        handleProvisioning();        break;
        case CloudState::AwaitingClaim:       handleAwaitingClaim();       break;
        case CloudState::Ready:               handleReady();               break;
        case CloudState::MqttConnecting:      handleMqttConnecting();      break;
        case CloudState::Online:              handleOnline();              break;
        default: break;
    }
}

void CloudStateMachine::handleWifiConnecting() {
    if (wifi_->isConnected()) {
        char ip[IDRYER_MAX_IP_LEN];
        wifi_->getLocalIP(ip, sizeof(ip));
        HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm", ip, wifi_->getRSSI());

        if (!identity_.hasSerialNumber() || (config_.waitForMcuSerial && !serialVerified_)) {
            setState(CloudState::WaitingForMcuSerial);
            return;
        }

        if (identity_.hasToken()) {
            setState(identity_.hasDeviceId() ? CloudState::Ready : CloudState::Provisioning);
        } else {
            setState(CloudState::Provisioning);
        }
        return;
    }

    const uint32_t now = HAL_MILLIS();
    if (now - lastWifiAttempt_ < config_.wifiRetryIntervalMs) return;
    lastWifiAttempt_ = now;

    HAL_LOG_INFO("CLOUD", "Connecting to WiFi...");
    wifi_->connect();
}

void CloudStateMachine::handleWaitingForMcuSerial() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    // Waiting for setMcuSerial() call
}

void CloudStateMachine::handleProvisioning() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }

    if (identity_.hasToken()) {
        if (identity_.hasDeviceId()) {
            setState(CloudState::Ready);
        } else if (!unclaimedNotified_) {
            unclaimedNotified_ = true;
            HAL_LOG_WARN("CLOUD", "Device NOT claimed (token exists). Waiting for claim request.");
            if (unclaimedCallback_) unclaimedCallback_(unclaimedCtx_);
        }
        return;
    }

    const uint32_t now = HAL_MILLIS();
    if (now - lastProvisionAttempt_ < config_.provisionRetryMs) return;
    lastProvisionAttempt_ = now;

    HAL_LOG_INFO("CLOUD", "Provisioning device...");
    ProvisionResult result = api_->provision(identity_.serialNumber);

    if (!result.success) { HAL_LOG_WARN("CLOUD", "Provision failed"); return; }

    if (result.isClaimed && result.token[0] == '\0') {
        HAL_LOG_WARN("CLOUD", "Serial claimed but token withheld. Delete device in app and re-claim.");
        if (unclaimedCallback_) unclaimedCallback_(unclaimedCtx_);
        return;
    }

    identity_.setToken(result.token);
    if (result.isClaimed && result.deviceId[0] != '\0') identity_.setDeviceId(result.deviceId);
    store_->save(identity_);

    HAL_LOG_INFO("CLOUD", "Provision OK: isNew=%d isClaimed=%d", result.isNew, result.isClaimed);

    if (identity_.hasDeviceId()) {
        setState(CloudState::Ready);
    } else {
        HAL_LOG_WARN("CLOUD", "Device NOT claimed. Waiting for claim request.");
        if (unclaimedCallback_) unclaimedCallback_(unclaimedCtx_);
    }
}

void CloudStateMachine::handleAwaitingClaim() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); awaitingClaim_ = false; return; }
    if (identity_.hasDeviceId()) { awaitingClaim_ = false; setState(CloudState::Ready); return; }

    const uint32_t now = HAL_MILLIS();
    if (now - lastClaimPoll_ < config_.claimPollIntervalMs) return;
    lastClaimPoll_ = now;

    ClaimCheckResult result = api_->checkClaim(identity_.token);
    if (!result.success || !result.claimed) return;

    identity_.setDeviceId(result.deviceId);
    store_->save(identity_);
    awaitingClaim_ = false;

    HAL_LOG_INFO("CLOUD", "Device claimed! deviceId=%s", identity_.deviceId);
    if (claimCompleteCallback_) claimCompleteCallback_(identity_.deviceId, claimCompleteCtx_);
    setState(CloudState::Ready);
}

void CloudStateMachine::handleReady() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    if (!identity_.hasToken() || !identity_.hasDeviceId()) { setState(CloudState::Provisioning); return; }
    setState(CloudState::MqttConnecting);
}

void CloudStateMachine::handleMqttConnecting() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    if (mqtt_->isConnected()) { HAL_LOG_INFO("CLOUD", "MQTT connected!"); setState(CloudState::Online); return; }

    const uint32_t now = HAL_MILLIS();
    if (now - lastMqttAttempt_ < config_.mqttRetryIntervalMs) return;
    lastMqttAttempt_ = now;

    HAL_LOG_INFO("CLOUD", "Connecting to MQTT...");

    if (!mqttInitialized_) {
        mqtt_->begin(identity_.serialNumber, identity_.token);
        mqttInitialized_ = true;
    }
    mqtt_->connect();
}

void CloudStateMachine::handleOnline() {
    if (!wifi_->isConnected()) { setState(CloudState::WifiConnecting); return; }
    if (!mqtt_->isConnected()) { setState(CloudState::MqttConnecting); return; }
    mqtt_->loop();
}

bool CloudStateMachine::requestClaim() {
    if (identity_.hasDeviceId()) { HAL_LOG_WARN("CLOUD", "Already claimed: %s", identity_.deviceId); return false; }
    if (!wifi_->isConnected()) { HAL_LOG_ERROR("CLOUD", "WiFi not connected"); return false; }

    if (!identity_.hasToken()) {
        HAL_LOG_INFO("CLOUD", "No token, doing provision first...");
        ProvisionResult provResult = api_->provision(identity_.serialNumber);
        if (!provResult.success) { HAL_LOG_ERROR("CLOUD", "Provision failed"); return false; }
        if (provResult.isClaimed && provResult.token[0] == '\0') {
            HAL_LOG_WARN("CLOUD", "Serial claimed but token withheld. Delete device in app first.");
            return false;
        }
        identity_.setToken(provResult.token);
        store_->save(identity_);
        if (provResult.isClaimed && provResult.deviceId[0] != '\0') {
            identity_.setDeviceId(provResult.deviceId);
            store_->save(identity_);
            HAL_LOG_INFO("CLOUD", "Already claimed: %s", identity_.deviceId);
            return true;
        }
    }

    if (awaitingClaim_) {
        HAL_LOG_INFO("CLOUD", "Claim already in progress, PIN=%s", pendingPin_);
        if (claimPinCallback_ && pendingPin_[0] != '\0') {
            uint32_t elapsedSec = (HAL_MILLIS() - pinCreatedAtMs_) / 1000;
            uint32_t remaining  = (elapsedSec < pinTotalSeconds_) ? (pinTotalSeconds_ - elapsedSec) : 0;
            claimPinCallback_(pendingPin_, remaining, claimPinCtx_);
        }
        return true;
    }

    HAL_LOG_INFO("CLOUD", "Registering device for claim...");
    RegisterResult regResult = api_->registerDevice(identity_.token, identity_.serialNumber);
    if (!regResult.success) { HAL_LOG_ERROR("CLOUD", "Register failed"); return false; }

    if (regResult.alreadyClaimed && regResult.deviceId[0] != '\0') {
        HAL_LOG_INFO("CLOUD", "Recovery: device already claimed, deviceId=%s", regResult.deviceId);
        identity_.setDeviceId(regResult.deviceId);
        store_->save(identity_);
        setState(CloudState::Ready);
        return true;
    }

    strncpy(pendingPin_, regResult.pin, sizeof(pendingPin_) - 1);
    pendingPin_[sizeof(pendingPin_)-1] = '\0';
    pinCreatedAtMs_  = HAL_MILLIS();
    pinTotalSeconds_ = regResult.remainingSeconds;
    awaitingClaim_ = true;
    lastClaimPoll_ = HAL_MILLIS() - config_.claimPollIntervalMs;

    HAL_LOG_INFO("CLOUD", "PIN: %s (expires in %us)", pendingPin_, regResult.remainingSeconds);
    if (claimPinCallback_) claimPinCallback_(pendingPin_, regResult.remainingSeconds, claimPinCtx_);

    setState(CloudState::AwaitingClaim);
    return true;
}

void CloudStateMachine::setMcuSerial(const char* mcuSerial) {
    if (!mcuSerial || mcuSerial[0] == '\0') return;
    if (serialVerified_ && strcmp(identity_.serialNumber, mcuSerial) == 0) return;

    if (identity_.hasSerialNumber() && strcmp(identity_.serialNumber, mcuSerial) != 0) {
        HAL_LOG_WARN("CLOUD", "MCU serial MISMATCH: NVS=%s UART=%s", identity_.serialNumber, mcuSerial);
        serialVerified_ = false;
        if (mqtt_->isConnected()) { mqtt_->disconnect(); mqttInitialized_ = false; }
        if (unclaimedCallback_) unclaimedCallback_(unclaimedCtx_);
        return;
    }

    identity_.setSerialNumber(mcuSerial);
    serialVerified_ = true;
    store_->save(identity_);
    HAL_LOG_INFO("CLOUD", "mcuSerial verified: %s", mcuSerial);

    if (state_ == CloudState::WaitingForMcuSerial) {
        lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
        setState(CloudState::Provisioning);
    }
}

bool CloudStateMachine::refreshToken() {
    if (!wifi_->isConnected()) { HAL_LOG_WARN("CLOUD", "refreshToken: no WiFi"); return false; }
    if (!identity_.hasSerialNumber()) { HAL_LOG_WARN("CLOUD", "refreshToken: no serial"); return false; }

    const uint32_t now = HAL_MILLIS();
    if (lastTokenRefreshMs_ != 0 && now - lastTokenRefreshMs_ < 30000u) {
        HAL_LOG_INFO("CLOUD", "refreshToken: cooldown active");
        return false;
    }
    lastTokenRefreshMs_ = now;

    ProvisionResult result = api_->provision(identity_.serialNumber);
    if (!result.success || result.token[0] == '\0') { HAL_LOG_WARN("CLOUD", "refreshToken: provision failed"); return false; }

    identity_.setToken(result.token);
    store_->save(identity_);
    HAL_LOG_INFO("CLOUD", "refreshToken: token updated");
    return true;
}

void CloudStateMachine::setState(CloudState newState) {
    if (state_ == newState) return;
    CloudState oldState = state_;
    state_ = newState;
    HAL_LOG_INFO("CLOUD", "State: %s -> %s", cloudStateToString(oldState), cloudStateToString(newState));
    if (stateCallback_) stateCallback_(oldState, newState, stateCallbackCtx_);
}

void CloudStateMachine::setStateChangeCallback(CloudStateChangeCallback cb, void* ctx) {
    stateCallback_ = cb; stateCallbackCtx_ = ctx;
}
void CloudStateMachine::setClaimPinCallback(ClaimPinCallback cb, void* ctx) {
    claimPinCallback_ = cb; claimPinCtx_ = ctx;
}
void CloudStateMachine::setClaimCompleteCallback(ClaimCompleteCallback cb, void* ctx) {
    claimCompleteCallback_ = cb; claimCompleteCtx_ = ctx;
}
void CloudStateMachine::setUnclaimedCallback(UnclaimedCallback cb, void* ctx) {
    unclaimedCallback_ = cb; unclaimedCtx_ = ctx;
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
