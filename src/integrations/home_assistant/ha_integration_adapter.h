/**
 * @file ha_integration_adapter.h
 * @brief Адаптер HA-интеграции: применение `HaConfig` из NVS к `HaMqttClient`.
 *
 * `HaMqttClient` уже умеет всё: mDNS-discover (`discover(host)`),
 * ручной `setServer(ip,port)`, `connect(clientId, username, password)`.
 * Этот адаптер добавляет:
 *   - state machine (Disabled / Idle / Connecting / Connected / Error),
 *   - reconnect с backoff,
 *   - применение credentials из `HaConfig`.
 *
 * @see docs/ru/07-features/04-home-assistant.md
 */

#pragma once

#include "../common/link_integrations_types.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_mqtt_client.h"
#include <functional>

namespace idryer {
namespace cloud {

/// Runtime-состояние адаптера HA-интеграции.
enum class HaConnectionState : uint8_t
{
    Disabled,     // configure() не вызывался или shutdown()
    Idle,         // настроен, но enabled=false
    Connecting,   // идёт discover / connect / reconnect
    Connected,    // PubSubClient активен
    Error,        // HA не найден / auth rejected
};

const char* haConnectionStateToString(HaConnectionState value);

class HaIntegrationAdapter
{
public:
    using StateChangeCallback = std::function<void(HaConnectionState)>;

    HaIntegrationAdapter();
    ~HaIntegrationAdapter();

    // Жизненный цикл ---------------------------------------------------------

    void setClientId(const char* clientId);
    void configure(const HaConfig& cfg);
    void shutdown();
    void loop();

    // Наблюдение -------------------------------------------------------------

    HaConnectionState state() const { return state_; }
    bool isConnected() const { return state_ == HaConnectionState::Connected; }
    const char* lastError() const { return lastError_; }

    ha::HaMqttClient* mqttClient() { return &client_; }

    void setStateChangeCallback(StateChangeCallback cb) { stateCallback_ = std::move(cb); }

    bool authConfigured() const { return cfg_.username[0] != '\0'; }

private:
    static constexpr uint32_t kReconnectMinMs = 1000;
    static constexpr uint32_t kReconnectMaxMs = 60000;

    void attemptConnect();
    void setState(HaConnectionState newState);
    void setError(const char* message);
    bool isIpAddress(const char* s) const;

    HaConfig          cfg_{};
    bool              configValid_ = false;
    HaConnectionState state_       = HaConnectionState::Disabled;
    char              lastError_[96] = {0};
    char              clientId_[48]  = {0};

    ha::HaMqttClient     client_;
    uint32_t             reconnectBackoffMs_   = kReconnectMinMs;
    uint32_t             nextConnectAttemptMs_ = 0;
    StateChangeCallback  stateCallback_;
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
