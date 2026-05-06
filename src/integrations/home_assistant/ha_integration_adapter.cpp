/**
 * @file ha_integration_adapter.cpp
 * @brief Реализация HaIntegrationAdapter.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_integration_adapter.h"
#include "../../hal/hal_types.h"
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

namespace idryer {
namespace cloud {

const char* haConnectionStateToString(HaConnectionState value)
{
    switch (value)
    {
    case HaConnectionState::Disabled:   return "disabled";
    case HaConnectionState::Idle:       return "idle";
    case HaConnectionState::Connecting: return "connecting";
    case HaConnectionState::Connected:  return "connected";
    case HaConnectionState::Error:      return "error";
    }
    return "disabled";
}

// =============================================================================
// Constructor / destructor
// =============================================================================

HaIntegrationAdapter::HaIntegrationAdapter() = default;

HaIntegrationAdapter::~HaIntegrationAdapter()
{
    shutdown();
}

// =============================================================================
// configure / shutdown
// =============================================================================

void HaIntegrationAdapter::setClientId(const char* clientId)
{
    if (!clientId) { clientId_[0] = '\0'; return; }
    strncpy(clientId_, clientId, sizeof(clientId_) - 1);
    clientId_[sizeof(clientId_) - 1] = '\0';
}

void HaIntegrationAdapter::configure(const HaConfig& cfg)
{
    bool identical =
        configValid_
        && cfg.enabled == cfg_.enabled
        && cfg.port    == cfg_.port
        && strcmp(cfg.host,     cfg_.host)     == 0
        && strcmp(cfg.username, cfg_.username) == 0
        && strcmp(cfg.password, cfg_.password) == 0;

    if (identical) return;

    cfg_ = cfg;
    configValid_ = cfg_.configured();

    if (!cfg_.enabled || !configValid_) {
        setState(HaConnectionState::Idle);
        if (!configValid_) setError("HA host is empty");
        else               setError("");
        return;
    }

    reconnectBackoffMs_ = kReconnectMinMs;
    nextConnectAttemptMs_ = millis();
    setState(HaConnectionState::Connecting);
    setError("");

    HAL_LOG_INFO("HA", "configure: host=%s port=%u auth=%s",
                 cfg_.host, cfg_.port, cfg_.username[0] ? "yes" : "no");
}

void HaIntegrationAdapter::shutdown()
{
    configValid_ = false;
    setState(HaConnectionState::Disabled);
}

// =============================================================================
// loop / reconnect
// =============================================================================

void HaIntegrationAdapter::loop()
{
    if (state_ == HaConnectionState::Disabled
        || state_ == HaConnectionState::Idle)
    {
        return;
    }

    if (client_.isConnected()) {
        client_.loop();
        return;
    }

    uint32_t now = millis();
    if (now < nextConnectAttemptMs_) return;

    if (state_ != HaConnectionState::Connecting) {
        setState(HaConnectionState::Connecting);
    }
    attemptConnect();
}

void HaIntegrationAdapter::attemptConnect()
{
    if (WiFi.status() != WL_CONNECTED) {
        setError("no wifi");
        nextConnectAttemptMs_ = millis() + 2000;
        return;
    }

    if (!clientId_[0]) {
        setError("clientId not set");
        setState(HaConnectionState::Error);
        return;
    }

    IPAddress ip;
    if (isIpAddress(cfg_.host) && ip.fromString(cfg_.host)) {
        client_.setServer(ip, cfg_.port);
    } else {
        if (!client_.discover(cfg_.host)) {
            setError("discover failed (HA not reachable)");
            nextConnectAttemptMs_ = millis() + reconnectBackoffMs_;
            reconnectBackoffMs_ *= 2;
            if (reconnectBackoffMs_ > kReconnectMaxMs) reconnectBackoffMs_ = kReconnectMaxMs;
            return;
        }
    }

    const char* user = cfg_.username[0] ? cfg_.username : nullptr;
    const char* pass = cfg_.password[0] ? cfg_.password : nullptr;

    HAL_LOG_INFO("HA", "connect attempt: %s:%u auth=%s",
                 cfg_.host, cfg_.port, user ? "yes" : "no");

    bool ok = client_.connect(clientId_, user, pass);

    if (ok) {
        setState(HaConnectionState::Connected);
        setError("");
        reconnectBackoffMs_ = kReconnectMinMs;
        HAL_LOG_INFO("HA", "connected");
        return;
    }

    setError("connect failed (check credentials or broker)");
    HAL_LOG_WARN("HA", "connect failed (backoff=%ums)", reconnectBackoffMs_);
    nextConnectAttemptMs_ = millis() + reconnectBackoffMs_;
    reconnectBackoffMs_ *= 2;
    if (reconnectBackoffMs_ > kReconnectMaxMs) reconnectBackoffMs_ = kReconnectMaxMs;
}

// =============================================================================
// Helpers
// =============================================================================

void HaIntegrationAdapter::setState(HaConnectionState newState)
{
    if (state_ == newState) return;
    HaConnectionState old = state_;
    state_ = newState;
    HAL_LOG_INFO("HA", "state: %s -> %s",
                 haConnectionStateToString(old),
                 haConnectionStateToString(newState));
    if (stateCallback_) stateCallback_(newState);
}

void HaIntegrationAdapter::setError(const char* message)
{
    if (!message) message = "";
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}

bool HaIntegrationAdapter::isIpAddress(const char* s) const
{
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p) {
        if (*p != '.' && (*p < '0' || *p > '9')) return false;
    }
    return true;
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
