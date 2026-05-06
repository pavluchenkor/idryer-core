/**
 * @file ha_mqtt_client.h
 * @brief MQTT клиент для Home Assistant (локальный брокер)
 *
 * Автоматически ищет HA через mDNS (homeassistant.local)
 * и подключается к локальному Mosquitto брокеру.
 */

#pragma once

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <functional>

namespace idryer {
namespace ha {

/// Максимальные размеры буферов для HA MQTT
#define HA_MQTT_BUFFER_SIZE 2048
#define HA_TOPIC_BUFFER_SIZE 128
#define HA_MQTT_KEEPALIVE 60

/// Результат mDNS поиска
struct HaDiscoveryResult {
    bool found = false;
    IPAddress ip;
    uint16_t port = 1883;  // Стандартный порт Mosquitto
};

/**
 * @brief MQTT клиент для Home Assistant
 *
 * Особенности:
 * - Автопоиск через mDNS
 * - Простая авторизация (username/password опционально)
 * - Публикация в простые топики
 * - Без TLS (локальная сеть)
 */
class HaMqttClient {
public:
    HaMqttClient();

    /**
     * @brief Поиск Home Assistant в локальной сети
     * @param host Если задан — используется напрямую (без mDNS). Дефолт: homeassistant.local
     * @return true если HA найден
     */
    bool discover(const char* host = nullptr);

    /**
     * @brief Ручная настройка адреса брокера (альтернатива discover)
     */
    void setServer(IPAddress ip, uint16_t port = 1883);

    /**
     * @brief Подключение к MQTT брокеру
     */
    bool connect(const char* clientId,
                 const char* username = nullptr,
                 const char* password = nullptr);

    bool isConnected();
    bool isDiscovered() const { return discovered_; }

    void loop();

    bool publish(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic);

    using MessageCallback = std::function<void(const char* topic, const char* payload)>;
    void setMessageCallback(MessageCallback cb) { messageCallback_ = cb; }

    const HaDiscoveryResult& getDiscoveryResult() const { return discoveryResult_; }

private:
    WiFiClient wifiClient_;
    PubSubClient mqttClient_;

    HaDiscoveryResult discoveryResult_;
    bool discovered_ = false;
    bool initialized_ = false;

    char clientId_[32];
    char username_[64];
    char password_[64];

    MessageCallback messageCallback_;

    char topicBuffer_[HA_TOPIC_BUFFER_SIZE];
};

} // namespace ha
} // namespace idryer

#endif // ESP32
