/**
 * @file ha_mqtt_client.cpp
 * @brief Реализация MQTT клиента для Home Assistant.
 *
 * Портировано из legacy idryer-protocol/src/mqtt/ha_mqtt_client.cpp
 * без изменений API. Единственное отличие — путь include на hal/hal_types.h
 * (файл переехал в integrations/home_assistant/, hal лежит на два уровня выше).
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_mqtt_client.h"
#include "../../hal/hal_types.h"
#include <string.h>

namespace idryer {
namespace ha {

HaMqttClient::HaMqttClient() {
    mqttClient_.setClient(wifiClient_);
    mqttClient_.setBufferSize(HA_MQTT_BUFFER_SIZE);
    mqttClient_.setKeepAlive(HA_MQTT_KEEPALIVE);

    mqttClient_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
        if (messageCallback_) {
            char buf[256];
            unsigned int len = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
            memcpy(buf, payload, len);
            buf[len] = '\0';
            messageCallback_(topic, buf);
        }
    });

    memset(clientId_, 0, sizeof(clientId_));
    memset(username_, 0, sizeof(username_));
    memset(password_, 0, sizeof(password_));
    memset(topicBuffer_, 0, sizeof(topicBuffer_));
}

bool HaMqttClient::discover(const char* host) {
    HAL_LOG_INFO("HA_MQTT", "Searching for Home Assistant...");

    IPAddress haIP;

    // Если host задан явно — используем его напрямую.
    // hostByName() на ESP32 не резолвит mDNS-имена (.local), поэтому при
    // failure фолбэчим на MDNS.queryService("home-assistant","tcp").
    if (host && host[0] != '\0') {
        HAL_LOG_INFO("HA_MQTT", "Using configured host: %s", host);
        WiFi.hostByName(host, haIP);
        if (haIP == INADDR_NONE) {
            HAL_LOG_WARN("HA_MQTT", "hostByName failed for %s, trying mDNS service query", host);
            delay(200);
            int n = MDNS.queryService("home-assistant", "tcp");
            if (n > 0) {
                haIP = MDNS.IP(0);
                HAL_LOG_INFO("HA_MQTT", "Found via mDNS service: %s", haIP.toString().c_str());
            }
        }
        if (haIP == INADDR_NONE) {
            HAL_LOG_WARN("HA_MQTT", "Cannot resolve host: %s", host);
            return false;
        }
    } else {
        // Автообнаружение через mDNS service discovery.
        // MDNS.begin() уже вызван transport-слоем (LocalAccess::initMdns) —
        // повторно вызывать нельзя, иначе перепишется hostname устройства.
        delay(200);
        int n = MDNS.queryService("home-assistant", "tcp");
        HAL_LOG_INFO("HA_MQTT", "mDNS service query result: %d", n);
        if (n > 0) {
            haIP = MDNS.IP(0);
            HAL_LOG_INFO("HA_MQTT", "Found _home-assistant._tcp: %s", haIP.toString().c_str());
        }
    }

    if (haIP == INADDR_NONE) {
        HAL_LOG_WARN("HA_MQTT", "Home Assistant not found");
        discoveryResult_.found = false;
        discovered_ = false;
        return false;
    }

    discoveryResult_.found = true;
    discoveryResult_.ip = haIP;
    discoveryResult_.port = 1883;
    discovered_ = true;

    HAL_LOG_INFO("HA_MQTT", "HA at %s:1883", haIP.toString().c_str());
    mqttClient_.setServer(discoveryResult_.ip, discoveryResult_.port);
    return true;
}

void HaMqttClient::setServer(IPAddress ip, uint16_t port) {
    discoveryResult_.found = true;
    discoveryResult_.ip = ip;
    discoveryResult_.port = port;
    discovered_ = true;

    mqttClient_.setServer(ip, port);

    HAL_LOG_INFO("HA_MQTT", "Server set manually: %s:%d",
                 ip.toString().c_str(), port);
}

bool HaMqttClient::connect(const char* clientId,
                           const char* username,
                           const char* password) {
    if (!discovered_) {
        HAL_LOG_ERROR("HA_MQTT", "Cannot connect: HA not discovered");
        return false;
    }
    if (!clientId || clientId[0] == '\0') {
        HAL_LOG_ERROR("HA_MQTT", "Cannot connect: clientId is empty");
        return false;
    }

    strncpy(clientId_, clientId, sizeof(clientId_) - 1);
    clientId_[sizeof(clientId_) - 1] = '\0';

    if (username && username[0] != '\0') {
        strncpy(username_, username, sizeof(username_) - 1);
        username_[sizeof(username_) - 1] = '\0';
    }
    if (password && password[0] != '\0') {
        strncpy(password_, password, sizeof(password_) - 1);
        password_[sizeof(password_) - 1] = '\0';
    }

    bool connected = false;
    if (username && username[0] != '\0') {
        connected = mqttClient_.connect(clientId_, username, password);
        HAL_LOG_INFO("HA_MQTT", "Connecting with auth: %s@%s:%d",
                     clientId_, discoveryResult_.ip.toString().c_str(),
                     discoveryResult_.port);
    } else {
        connected = mqttClient_.connect(clientId_);
        HAL_LOG_INFO("HA_MQTT", "Connecting without auth: %s@%s:%d",
                     clientId_, discoveryResult_.ip.toString().c_str(),
                     discoveryResult_.port);
    }

    if (connected) {
        HAL_LOG_INFO("HA_MQTT", "Connected to Home Assistant MQTT");
        initialized_ = true;
    } else {
        int state = mqttClient_.state();
        HAL_LOG_ERROR("HA_MQTT", "Connection failed, state=%d", state);
    }

    return connected;
}

bool HaMqttClient::isConnected() {
    return initialized_ && mqttClient_.connected();
}

void HaMqttClient::loop() {
    if (!initialized_) return;
    mqttClient_.loop();

    // Автореконнект при потере связи — раз в 5 секунд.
    if (!mqttClient_.connected() && discovered_) {
        static uint32_t lastReconnectAttempt = 0;
        uint32_t now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            HAL_LOG_WARN("HA_MQTT", "Reconnecting...");
            connect(clientId_,
                    username_[0] != '\0' ? username_ : nullptr,
                    password_[0] != '\0' ? password_ : nullptr);
        }
    }
}

bool HaMqttClient::subscribe(const char* topic) {
    if (!isConnected() || !topic) return false;
    bool ok = mqttClient_.subscribe(topic);
    HAL_LOG_INFO("HA_MQTT", "Subscribe %s: %s", topic, ok ? "OK" : "FAIL");
    return ok;
}

bool HaMqttClient::publish(const char* topic, const char* payload, bool retained) {
    if (!isConnected()) return false;
    if (!topic || !payload) {
        HAL_LOG_ERROR("HA_MQTT", "Cannot publish: topic or payload is NULL");
        return false;
    }

    bool success = mqttClient_.publish(topic, payload, retained);
    if (success) {
        HAL_LOG_DEBUG("HA_MQTT", "Published: %s = %s", topic, payload);
    } else {
        HAL_LOG_ERROR("HA_MQTT", "Publish failed: %s", topic);
    }
    return success;
}

} // namespace ha
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
