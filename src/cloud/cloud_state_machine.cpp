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
    pendingPin_[0]         = '\0';
    mcuSerial_[0]          = '\0';
    mcuFirmwareVersion_[0] = '\0';
    mqttKey_[0]            = '\0';
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
    if (serialVerified_) {
        lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
        setState(CloudState::Provisioning);
    }
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

    emitDiagnostic("claim: checking backend");
    ClaimCheckResult result = api_->checkClaim(identity_.token);
    if (!result.success || !result.claimed) return;

    identity_.setDeviceId(result.deviceId);
    store_->save(identity_);
    awaitingClaim_ = false;

    HAL_LOG_INFO("CLOUD", "Device claimed! deviceId=%s", identity_.deviceId);
    emitDiagnostic2("claim: backend confirmed deviceId=", identity_.deviceId);
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
        const char* key = (mqttKey_[0] != '\0') ? mqttKey_ : identity_.serialNumber;
        mqtt_->begin(key, identity_.token);
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
    idryer::ClaimRequestResult result = requestClaimDetailed();
    return result == idryer::ClaimRequestResult::Started ||
           result == idryer::ClaimRequestResult::AlreadyClaimed;
}

idryer::ClaimRequestResult CloudStateMachine::requestClaimDetailed() {
    if (identity_.hasDeviceId()) {
        HAL_LOG_WARN("CLOUD", "Local NVS has deviceId=%s, checking backend claim state...", identity_.deviceId);
        emitDiagnostic2("claim: local NVS has deviceId=", identity_.deviceId);
        emitDiagnostic("claim: checking backend");
        if (!identity_.hasToken()) {
            HAL_LOG_WARN("CLOUD", "Stale claim NVS: deviceId exists but token is missing");
            emitDiagnostic("claim: stale NVS, token is missing");
            return idryer::ClaimRequestResult::StaleNvs;
        }

        ClaimCheckResult claim = api_->checkClaim(identity_.token);
        if (claim.success && claim.claimed) {
            if (claim.deviceId[0] != '\0' && strcmp(claim.deviceId, identity_.deviceId) != 0) {
                identity_.setDeviceId(claim.deviceId);
                store_->save(identity_);
            }
            HAL_LOG_INFO("CLOUD", "Backend confirms claim: deviceId=%s", identity_.deviceId);
            emitDiagnostic2("claim: backend confirmed deviceId=", identity_.deviceId);
            return idryer::ClaimRequestResult::AlreadyClaimed;
        }

        HAL_LOG_WARN("CLOUD", "Stale claim NVS: backend did not confirm deviceId=%s", identity_.deviceId);
        emitDiagnostic2("claim: stale NVS, backend did not confirm deviceId=", identity_.deviceId);
        return idryer::ClaimRequestResult::StaleNvs;
    }

    if (!wifi_->isConnected()) { HAL_LOG_ERROR("CLOUD", "WiFi not connected"); return idryer::ClaimRequestResult::WifiNotConnected; }
    if (config_.waitForMcuSerial && !serialVerified_) {
        HAL_LOG_WARN("CLOUD", "Claim rejected: waiting for MCU serial");
        return idryer::ClaimRequestResult::WaitingForMcuSerial;
    }

    if (!identity_.hasToken()) {
        HAL_LOG_INFO("CLOUD", "No token, doing provision first...");
        ProvisionResult provResult = api_->provision(identity_.serialNumber);
        if (!provResult.success) { HAL_LOG_ERROR("CLOUD", "Provision failed"); return idryer::ClaimRequestResult::ProvisionFailed; }
        if (provResult.isClaimed && provResult.token[0] == '\0') {
            HAL_LOG_WARN("CLOUD", "Serial claimed but token withheld. Delete device in app first.");
            return idryer::ClaimRequestResult::TokenWithheld;
        }
        identity_.setToken(provResult.token);
        store_->save(identity_);
        if (provResult.isClaimed && provResult.deviceId[0] != '\0') {
            identity_.setDeviceId(provResult.deviceId);
            store_->save(identity_);
            HAL_LOG_INFO("CLOUD", "Already claimed: %s", identity_.deviceId);
            return idryer::ClaimRequestResult::AlreadyClaimed;
        }
    }

    if (awaitingClaim_) {
        HAL_LOG_INFO("CLOUD", "Claim already in progress, PIN=%s", pendingPin_);
        if (claimPinCallback_ && pendingPin_[0] != '\0') {
            uint32_t elapsedSec = (HAL_MILLIS() - pinCreatedAtMs_) / 1000;
            uint32_t remaining  = (elapsedSec < pinTotalSeconds_) ? (pinTotalSeconds_ - elapsedSec) : 0;
            claimPinCallback_(pendingPin_, remaining, claimPinCtx_);
        }
        return idryer::ClaimRequestResult::Started;
    }

    HAL_LOG_INFO("CLOUD", "Registering device for claim...");
    emitDiagnostic("claim: requesting PIN from backend");
    RegisterResult regResult = api_->registerDevice(identity_.token, identity_.serialNumber);
    if (!regResult.success) { HAL_LOG_ERROR("CLOUD", "Register failed"); return idryer::ClaimRequestResult::RegisterFailed; }

    if (regResult.alreadyClaimed && regResult.deviceId[0] != '\0') {
        HAL_LOG_INFO("CLOUD", "Recovery: device already claimed, deviceId=%s", regResult.deviceId);
        identity_.setDeviceId(regResult.deviceId);
        store_->save(identity_);
        setState(CloudState::Ready);
        return idryer::ClaimRequestResult::AlreadyClaimed;
    }

    strncpy(pendingPin_, regResult.pin, sizeof(pendingPin_) - 1);
    pendingPin_[sizeof(pendingPin_)-1] = '\0';
    pinCreatedAtMs_  = HAL_MILLIS();
    pinTotalSeconds_ = regResult.remainingSeconds;
    awaitingClaim_ = true;
    lastClaimPoll_ = HAL_MILLIS() - config_.claimPollIntervalMs;

    HAL_LOG_INFO("CLOUD", "PIN: %s (expires in %us)", pendingPin_, regResult.remainingSeconds);
    emitDiagnostic2("claim: PIN received pin=", pendingPin_);
    if (claimPinCallback_) claimPinCallback_(pendingPin_, regResult.remainingSeconds, claimPinCtx_);

    setState(CloudState::AwaitingClaim);
    return idryer::ClaimRequestResult::Started;
}

idryer::McuSerialResult CloudStateMachine::setMcuSerial(const char* mcuSerial) {
    if (!mcuSerial || mcuSerial[0] == '\0') {
        return idryer::McuSerialResult::Ignored;
    }

    if (!identity_.hasBoundMqttKey()) {
        // First bind: no prior bound key — accept and use linkSerial as mqttKey
        strncpy(mcuSerial_, mcuSerial, sizeof(mcuSerial_) - 1);
        mcuSerial_[sizeof(mcuSerial_) - 1] = '\0';
        strncpy(mqttKey_, identity_.serialNumber, sizeof(mqttKey_) - 1);
        mqttKey_[sizeof(mqttKey_) - 1] = '\0';
        bindSwitchPending_ = true;
        serialVerified_ = true;
        HAL_LOG_INFO("CLOUD", "mcuSerial accepted (first bind): linkSerial=%s mcuSerial=%s",
                     identity_.serialNumber, mcuSerial_);
        if (state_ == CloudState::WaitingForMcuSerial) {
            lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
            setState(CloudState::Provisioning);
        }
        return idryer::McuSerialResult::AcceptedFirstBind;
    }

    if (strcmp(identity_.boundMqttKey, mcuSerial) == 0) {
        // Already bound: same RP2040 — use boundMqttKey as mqttKey
        strncpy(mcuSerial_, mcuSerial, sizeof(mcuSerial_) - 1);
        mcuSerial_[sizeof(mcuSerial_) - 1] = '\0';
        strncpy(mqttKey_, identity_.boundMqttKey, sizeof(mqttKey_) - 1);
        mqttKey_[sizeof(mqttKey_) - 1] = '\0';
        bindSwitchPending_ = false;
        serialVerified_ = true;
        HAL_LOG_INFO("CLOUD", "mcuSerial accepted (already bound): mqttKey=%s", mqttKey_);
        if (state_ == CloudState::WaitingForMcuSerial) {
            lastProvisionAttempt_ = HAL_MILLIS() - config_.provisionRetryMs;
            setState(CloudState::Provisioning);
        }
        return idryer::McuSerialResult::AcceptedBound;
    }

    HAL_LOG_WARN("CLOUD", "mcuSerial MISMATCH: bound=%s uart=%s",
                 identity_.boundMqttKey, mcuSerial);
    return idryer::McuSerialResult::Mismatch;
}

bool CloudStateMachine::handleBindAck(const char* mqttTopicKey, const char* mcuSerial) {
    if (!mqttTopicKey || !mcuSerial) return false;

    // One-ID: mqttTopicKey matches linkSerial — no MQTT switch needed
    if (strcmp(mqttTopicKey, identity_.serialNumber) == 0) {
        HAL_LOG_INFO("CLOUD", "bind_ack accepted (one-ID): topic unchanged");
        bindSwitchPending_ = false;
        return true;
    }

    // Two-chip: verify against mcuSerial_ we received in UART Hello
    if (mcuSerial_[0] == '\0' ||
        strcmp(mqttTopicKey, mcuSerial_) != 0 ||
        strcmp(mcuSerial,    mcuSerial_) != 0) {
        HAL_LOG_WARN("CLOUD", "bind_ack rejected: expected=%s got mqttTopicKey=%s mcuSerial=%s",
                     mcuSerial_, mqttTopicKey, mcuSerial);
        return false;
    }

    // Save boundMqttKey to NVS and switch MQTT identity
    identity_.setBoundMqttKey(mqttTopicKey);
    store_->save(identity_);

    strncpy(mqttKey_, mqttTopicKey, sizeof(mqttKey_) - 1);
    mqttKey_[sizeof(mqttKey_) - 1] = '\0';
    bindSwitchPending_ = false;

    HAL_LOG_INFO("CLOUD", "bind_ack accepted: switching MQTT to mqttKey=%s", mqttKey_);

    if (mqtt_->isConnected()) mqtt_->disconnect();
    mqttInitialized_ = false;
    setState(CloudState::MqttConnecting);
    return true;
}

const char* CloudStateMachine::getMcuSerial() const {
    return (mcuSerial_[0] != '\0') ? mcuSerial_ : nullptr;
}

void CloudStateMachine::setMcuFirmwareVersion(uint32_t fwVersion) {
    snprintf(mcuFirmwareVersion_, sizeof(mcuFirmwareVersion_), "%u.%u.%u",
             (fwVersion >> 16) & 0xFF,
             (fwVersion >> 8)  & 0xFF,
             fwVersion         & 0xFF);
}

const char* CloudStateMachine::getMcuFirmwareVersion() const {
    return (mcuFirmwareVersion_[0] != '\0') ? mcuFirmwareVersion_ : nullptr;
}

const char* CloudStateMachine::getMqttKey() const {
    return (mqttKey_[0] != '\0') ? mqttKey_ : identity_.serialNumber;
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

void CloudStateMachine::setDiagnosticCallback(DiagnosticCallback cb, void* ctx) {
    diagnosticCallback_ = cb; diagnosticCtx_ = ctx;
}

void CloudStateMachine::emitDiagnostic(const char* message) {
    if (diagnosticCallback_ && message) diagnosticCallback_(message, diagnosticCtx_);
}

void CloudStateMachine::emitDiagnostic2(const char* prefix, const char* value) {
    if (!diagnosticCallback_ || !prefix) return;
    char line[128];
    snprintf(line, sizeof(line), "%s%s", prefix, value ? value : "");
    diagnosticCallback_(line, diagnosticCtx_);
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
