/**
 * @file ha_publisher.cpp
 * @brief Реализация публикатора для Home Assistant
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "ha_publisher.h"
#include "../../hal/hal_types.h"
#include <stdio.h>
#include <string.h>

namespace idryer {
namespace ha {

// =============================================================================
// КОНСТАНТЫ
// =============================================================================

namespace {
    constexpr const char* HA_PREFIX = "homeassistant";
    constexpr const char* DEVICE_NAME = "iDryer";
    constexpr const char* MANUFACTURER = "iDryer";
}

// =============================================================================
// КОНСТРУКТОР
// =============================================================================

HaPublisher::HaPublisher(HaMqttClient* mqtt)
    : mqtt_(mqtt) {
    memset(deviceId_, 0, sizeof(deviceId_));
    memset(hwVersion_, 0, sizeof(hwVersion_));
    memset(fwVersion_, 0, sizeof(fwVersion_));
    memset(topicBuf_, 0, sizeof(topicBuf_));
    memset(payloadBuf_, 0, sizeof(payloadBuf_));

    mqtt_->setMessageCallback([this](const char* topic, const char* payload) {
        handleIncomingMessage(topic, payload);
    });
}

// =============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// =============================================================================

void HaPublisher::formatUnitId(char* buffer, size_t size, uint8_t unitId) {
    snprintf(buffer, size, "U%d", unitId + 1);
}

const char* HaPublisher::modeToString(idryer::UartDryerMode mode) {
    switch (mode) {
        case idryer::UartDryerMode::Idle:    return "IDLE";
        case idryer::UartDryerMode::Drying:  return "DRYING";
        case idryer::UartDryerMode::Storage: return "STORAGE";
        case idryer::UartDryerMode::Profile: return "PROFILE";
        case idryer::UartDryerMode::Fault:   return "FAULT";
        default: return "UNKNOWN";
    }
}

void HaPublisher::makeStateTopic(char* buf, size_t size, uint8_t unitId, const char* sensorName) {
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(buf, size, "idryer/%s/%s/%s", deviceId_, unitStr, sensorName);
}

void HaPublisher::makeConfigTopic(char* buf, size_t size, const char* domain,
                                  uint8_t unitId, const char* sensorName) {
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(buf, size, "%s/%s/idryer_%s_%s_%s/config", HA_PREFIX, domain, deviceId_, unitStr, sensorName);
}

void HaPublisher::makeDeviceJson(char* buf, size_t size) {
    snprintf(buf, size,
        "\"device\":{"
            "\"identifiers\":[\"idryer_%s\"],"
            "\"name\":\"%s %s\","
            "\"model\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"sw_version\":\"%s\""
        "}",
        deviceId_,
        DEVICE_NAME, deviceId_,
        hwVersion_,
        MANUFACTURER,
        fwVersion_
    );
}

// =============================================================================
// DISCOVERY - СЕНСОРЫ
// =============================================================================

bool HaPublisher::publishSensorDiscovery(uint8_t unitId, const char* sensorName,
                                         const char* deviceClass, const char* unit,
                                         const char* icon) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    makeConfigTopic(topicBuf_, sizeof(topicBuf_), "sensor", unitId, sensorName);

    char stateTopic[HA_TOPIC_BUF_SIZE];
    makeStateTopic(stateTopic, sizeof(stateTopic), unitId, sensorName);

    char uniqueId[64];
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_%s", deviceId_, unitStr, sensorName);

    char name[64];
    snprintf(name, sizeof(name), "iDryer %s %s", unitStr, sensorName);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    int len = snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"%s\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\"",
        name, uniqueId, stateTopic
    );

    if (deviceClass) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"device_class\":\"%s\"", deviceClass);
    }

    if (unit) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"unit_of_measurement\":\"%s\"", unit);
    }

    if (icon) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"icon\":\"%s\"", icon);
    }

    len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
        ",%s}", deviceJson);

    bool success = mqtt_->publish(topicBuf_, payloadBuf_, true);

    if (success) {
        HAL_LOG_DEBUG("HA_PUB", "Discovery sent: %s", sensorName);
    } else {
        HAL_LOG_ERROR("HA_PUB", "Discovery failed: %s", sensorName);
    }

    return success;
}

bool HaPublisher::publishBinarySensorDiscovery(uint8_t unitId, const char* sensorName,
                                                const char* deviceClass,
                                                const char* icon) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    makeConfigTopic(topicBuf_, sizeof(topicBuf_), "binary_sensor", unitId, sensorName);

    char stateTopic[HA_TOPIC_BUF_SIZE];
    makeStateTopic(stateTopic, sizeof(stateTopic), unitId, sensorName);

    char uniqueId[64];
    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_%s", deviceId_, unitStr, sensorName);

    char name[64];
    snprintf(name, sizeof(name), "iDryer %s %s", unitStr, sensorName);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    int len = snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"%s\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"payload_on\":\"ON\","
            "\"payload_off\":\"OFF\"",
        name, uniqueId, stateTopic
    );

    if (deviceClass) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"device_class\":\"%s\"", deviceClass);
    }

    if (icon) {
        len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
            ",\"icon\":\"%s\"", icon);
    }

    len += snprintf(payloadBuf_ + len, sizeof(payloadBuf_) - len,
        ",%s}", deviceJson);

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

bool HaPublisher::publishAlertDiscovery() {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    snprintf(topicBuf_, sizeof(topicBuf_),
        "%s/sensor/idryer_%s_alerts/config", HA_PREFIX, deviceId_);

    char stateTopic[HA_TOPIC_BUF_SIZE];
    snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/alerts", deviceId_);

    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_alerts", deviceId_);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"iDryer %s Alerts\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"icon\":\"mdi:alert-circle\","
            "%s"
        "}",
        deviceId_, uniqueId, stateTopic, deviceJson
    );

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

// =============================================================================
// ПУБЛИКАЦИЯ DISCOVERY
// =============================================================================

bool HaPublisher::publishDiscovery(const char* deviceId, uint8_t unitsCount,
                                   const char* hwVersion, const char* fwVersion,
                                   int tempMin, int tempMax, int durationMax,
                                   const HaCapabilities& caps) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        HAL_LOG_ERROR("HA_PUB", "Cannot publish Discovery: MQTT not connected");
        return false;
    }

    if (discoveryPublished_) {
        HAL_LOG_WARN("HA_PUB", "Discovery already published");
        return true;
    }

    strncpy(deviceId_, deviceId, sizeof(deviceId_) - 1);
    strncpy(hwVersion_, hwVersion, sizeof(hwVersion_) - 1);
    strncpy(fwVersion_, fwVersion, sizeof(fwVersion_) - 1);
    unitsCount_ = unitsCount;
    tempMin_ = tempMin;
    tempMax_ = tempMax;
    durationMax_ = durationMax;

    HAL_LOG_INFO("HA_PUB", "Publishing Discovery for %s (%d units)", deviceId_, unitsCount_);

    for (uint8_t i = 0; i < unitsCount_ && i < 3; i++) {
        // Sensors — публикуем только то, что продукт реально поддерживает.
        // Отсутствующие — стираем (на случай если раньше публиковали retained).
        if (caps.airTemp) {
            if (!publishSensorDiscovery(i, "temperature", "temperature", "°C", "mdi:thermometer")) return false;
        } else {
            removeSensorDiscovery(i, "temperature");
        }
        if (caps.airHumidity) {
            if (!publishSensorDiscovery(i, "humidity", "humidity", "%", "mdi:water-percent")) return false;
        } else {
            removeSensorDiscovery(i, "humidity");
        }
        if (caps.heaterPower) {
            if (!publishSensorDiscovery(i, "heater_power", "power_factor", "%", "mdi:radiator")) return false;
        } else {
            removeSensorDiscovery(i, "heater_power");
        }
        if (caps.fan) {
            if (!publishBinarySensorDiscovery(i, "fan", nullptr, "mdi:fan")) return false;
        } else {
            removeSensorDiscovery(i, "fan", /*binary=*/true);
        }
        if (caps.weight) {
            if (!publishSensorDiscovery(i, "weight", "weight", "g", "mdi:weight-gram")) return false;
        } else {
            removeSensorDiscovery(i, "weight");
        }

        // mode + select + numbers — общие управляющие entity, всегда.
        if (!publishSensorDiscovery(i, "mode", nullptr, nullptr, "mdi:state-machine")) return false;
        if (!publishSelectDiscovery(i)) return false;
        if (!publishNumberDiscovery(i, "target temp", "set_temp", "target_temp",
                                    tempMin_, tempMax_, "°C", "mdi:thermometer-plus")) return false;
        if (!publishNumberDiscovery(i, "duration", "set_duration", "target_duration",
                                    10, durationMax_, "min", "mdi:timer-outline")) return false;
    }

    if (!publishAlertDiscovery()) return false;

    subscribeToCommands();

    discoveryPublished_ = true;
    HAL_LOG_INFO("HA_PUB", "Discovery published successfully");

    return true;
}

// =============================================================================
// ПУБЛИКАЦИЯ ДАННЫХ
// =============================================================================

bool HaPublisher::publishTelemetry(const idryer::UartTelemetryPayload& data) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    bool allSuccess = true;

    for (uint8_t i = 0; i < data.count && i < 3; i++) {
        const auto& entry = data.units[i];

        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "temperature");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", entry.temperatureC10 / 10.0f);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);

        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "humidity");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", entry.humidityPct10 / 10.0f);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);

        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "heater_power");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%d", entry.heaterPowerPct);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);

        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "fan");
        mqtt_->publish(topicBuf_, entry.fanOn ? "ON" : "OFF");
    }

    if (allSuccess) {
        HAL_LOG_DEBUG("HA_PUB", "Telemetry published (%d units)", data.count);
    }

    return allSuccess;
}

bool HaPublisher::publishStatus(const idryer::UartStatusPayload& data) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    bool allSuccess = true;

    for (uint8_t i = 0; i < data.count && i < 3; i++) {
        const auto& entry = data.units[i];

        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "mode");
        const char* modeStr = modeToString(entry.mode);
        allSuccess &= mqtt_->publish(topicBuf_, modeStr);
    }

    if (allSuccess) {
        HAL_LOG_DEBUG("HA_PUB", "Status published (%d units)", data.count);
    }

    return allSuccess;
}

bool HaPublisher::publishWeights(const idryer::UartWeightsPayload& data) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    bool allSuccess = true;

    for (uint8_t i = 0; i < data.count && i < 8; i++) {
        const auto& entry = data.weights[i];

        makeStateTopic(topicBuf_, sizeof(topicBuf_), entry.unitId, "weight");
        snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", entry.weightGramsC10 / 10.0f);
        allSuccess &= mqtt_->publish(topicBuf_, payloadBuf_);
    }

    if (allSuccess) {
        HAL_LOG_DEBUG("HA_PUB", "Weights published (%d sensors)", data.count);
    }

    return allSuccess;
}

bool HaPublisher::removeSensorDiscovery(uint8_t unitId, const char* sensorName, bool binary) {
    if (!mqtt_ || !mqtt_->isConnected()) return false;
    const char* domain = binary ? "binary_sensor" : "sensor";
    makeConfigTopic(topicBuf_, sizeof(topicBuf_), domain, unitId, sensorName);
    // Empty retained payload удаляет конфиг entity в HA Discovery.
    return mqtt_->publish(topicBuf_, "", /*retained=*/true);
}

bool HaPublisher::publishUnitState(uint8_t unitId,
                                    float temperatureC, float humidityPct,
                                    int heaterPowerPct, bool fanOn,
                                    const char* modeStr,
                                    float targetTempC, uint32_t targetDurMin) {
    if (!mqtt_ || !mqtt_->isConnected()) return false;
    if (!discoveryPublished_) return false;  // нет смысла публиковать state без entities

    bool ok = true;

    makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "temperature");
    snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", temperatureC);
    ok &= mqtt_->publish(topicBuf_, payloadBuf_);

    makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "humidity");
    snprintf(payloadBuf_, sizeof(payloadBuf_), "%.1f", humidityPct);
    ok &= mqtt_->publish(topicBuf_, payloadBuf_);

    makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "heater_power");
    snprintf(payloadBuf_, sizeof(payloadBuf_), "%d", heaterPowerPct);
    ok &= mqtt_->publish(topicBuf_, payloadBuf_);

    makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "fan");
    ok &= mqtt_->publish(topicBuf_, fanOn ? "ON" : "OFF");

    if (modeStr && modeStr[0]) {
        makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "mode");
        ok &= mqtt_->publish(topicBuf_, modeStr);
    }

    // target_temp/target_duration — state_topics для number entities в Discovery.
    makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "target_temp");
    snprintf(payloadBuf_, sizeof(payloadBuf_), "%d", (int)targetTempC);
    ok &= mqtt_->publish(topicBuf_, payloadBuf_);

    makeStateTopic(topicBuf_, sizeof(topicBuf_), unitId, "target_duration");
    snprintf(payloadBuf_, sizeof(payloadBuf_), "%u", (unsigned)targetDurMin);
    ok &= mqtt_->publish(topicBuf_, payloadBuf_);

    return ok;
}

bool HaPublisher::publishSelectDiscovery(uint8_t unitId) {
    if (!mqtt_ || !mqtt_->isConnected()) return false;

    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);

    snprintf(topicBuf_, sizeof(topicBuf_),
        "%s/select/idryer_%s_%s_%s/config", HA_PREFIX, deviceId_, unitStr, "mode_control");

    char stateTopic[HA_TOPIC_BUF_SIZE];
    makeStateTopic(stateTopic, sizeof(stateTopic), unitId, "mode");

    char cmdTopic[HA_TOPIC_BUF_SIZE];
    snprintf(cmdTopic, sizeof(cmdTopic), "idryer/%s/%s/set_mode", deviceId_, unitStr);

    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_mode_control", deviceId_, unitStr);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"iDryer %s mode control\","
            "\"unique_id\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"command_topic\":\"%s\","
            "\"options\":[\"IDLE\",\"DRYING\",\"STORAGE\"],"
            "\"icon\":\"mdi:washing-machine\","
            "%s"
        "}",
        unitStr, uniqueId, stateTopic, cmdTopic, deviceJson
    );

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

bool HaPublisher::publishNumberDiscovery(uint8_t unitId, const char* name,
                                         const char* cmdSuffix, const char* stateSuffix,
                                         int min, int max,
                                         const char* unit, const char* icon) {
    if (!mqtt_ || !mqtt_->isConnected()) return false;

    char unitStr[4];
    formatUnitId(unitStr, sizeof(unitStr), unitId);

    snprintf(topicBuf_, sizeof(topicBuf_),
        "%s/number/idryer_%s_%s_%s/config", HA_PREFIX, deviceId_, unitStr, cmdSuffix);

    char cmdTopic[HA_TOPIC_BUF_SIZE];
    snprintf(cmdTopic, sizeof(cmdTopic), "idryer/%s/%s/%s", deviceId_, unitStr, cmdSuffix);

    char stateTopic[HA_TOPIC_BUF_SIZE];
    snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/%s/%s", deviceId_, unitStr, stateSuffix);

    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "idryer_%s_%s_%s", deviceId_, unitStr, cmdSuffix);

    char deviceJson[256];
    makeDeviceJson(deviceJson, sizeof(deviceJson));

    snprintf(payloadBuf_, sizeof(payloadBuf_),
        "{"
            "\"name\":\"iDryer %s %s\","
            "\"unique_id\":\"%s\","
            "\"command_topic\":\"%s\","
            "\"state_topic\":\"%s\","
            "\"min\":%d,\"max\":%d,\"step\":1,"
            "\"unit_of_measurement\":\"%s\","
            "\"icon\":\"%s\","
            "%s"
        "}",
        unitStr, name, uniqueId, cmdTopic, stateTopic,
        min, max, unit, icon, deviceJson
    );

    return mqtt_->publish(topicBuf_, payloadBuf_, true);
}

bool HaPublisher::subscribeToCommands() {
    if (!mqtt_ || !mqtt_->isConnected()) return false;

    char topic[HA_TOPIC_BUF_SIZE];

    snprintf(topic, sizeof(topic), "idryer/%s/+/set_mode", deviceId_);
    mqtt_->subscribe(topic);

    snprintf(topic, sizeof(topic), "idryer/%s/+/set_temp", deviceId_);
    mqtt_->subscribe(topic);

    snprintf(topic, sizeof(topic), "idryer/%s/+/set_duration", deviceId_);
    mqtt_->subscribe(topic);

    return true;
}

void HaPublisher::handleIncomingMessage(const char* topic, const char* payload) {
    if (!topic || !payload) return;

    char prefix[HA_TOPIC_BUF_SIZE];
    snprintf(prefix, sizeof(prefix), "idryer/%s/", deviceId_);
    size_t prefixLen = strlen(prefix);

    if (strncmp(topic, prefix, prefixLen) != 0) return;

    const char* rest = topic + prefixLen;
    const char* slash = strchr(rest, '/');
    if (!slash) return;

    char unitStr[8];
    size_t unitLen = slash - rest;
    if (unitLen >= sizeof(unitStr)) return;
    memcpy(unitStr, rest, unitLen);
    unitStr[unitLen] = '\0';

    const char* suffix = slash + 1;

    uint8_t unitIdx = 0;
    if (unitStr[0] == 'U' && unitStr[1] >= '1' && unitStr[1] <= '4') {
        unitIdx = unitStr[1] - '1';
    }

    if (strcmp(suffix, "set_temp") == 0) {
        int val = atoi(payload);
        if (val >= 30 && val <= 85) {
            targetTempC_[unitIdx] = val;
            char stateTopic[HA_TOPIC_BUF_SIZE];
            snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/%s/target_temp", deviceId_, unitStr);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", val);
            mqtt_->publish(stateTopic, buf);
            HAL_LOG_INFO("HA_PUB", "Set temp %s: %d°C", unitStr, val);
        }
        return;
    }

    if (strcmp(suffix, "set_duration") == 0) {
        int val = atoi(payload);
        if (val >= 10 && val <= 1440) {
            targetDurMin_[unitIdx] = val;
            char stateTopic[HA_TOPIC_BUF_SIZE];
            snprintf(stateTopic, sizeof(stateTopic), "idryer/%s/%s/target_duration", deviceId_, unitStr);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", val);
            mqtt_->publish(stateTopic, buf);
            HAL_LOG_INFO("HA_PUB", "Set duration %s: %dmin", unitStr, val);
        }
        return;
    }

    if (strcmp(suffix, "set_mode") == 0 && commandCallback_) {
        const char* command = nullptr;
        if (strcmp(payload, "DRYING") == 0)       command = "drying";
        else if (strcmp(payload, "STORAGE") == 0) command = "storage";
        else if (strcmp(payload, "IDLE") == 0)    command = "stop";

        if (!command) {
            HAL_LOG_WARN("HA_PUB", "Unknown mode: %s", payload);
            return;
        }

        HAL_LOG_INFO("HA_PUB", "Command from HA: %s -> %s (temp=%d, dur=%d)",
                     unitStr, command, targetTempC_[unitIdx], targetDurMin_[unitIdx]);
        commandCallback_(command, unitStr, targetTempC_[unitIdx], targetDurMin_[unitIdx]);
    }
}

bool HaPublisher::publishAlert(uint8_t unitId, const char* message, const char* severity) {
    if (!mqtt_ || !mqtt_->isConnected()) {
        return false;
    }

    if (!message) {
        return false;
    }

    snprintf(topicBuf_, sizeof(topicBuf_), "idryer/%s/alerts", deviceId_);

    char unitStr[4];
    if (unitId != 0xFF) {
        formatUnitId(unitStr, sizeof(unitStr), unitId);
        snprintf(payloadBuf_, sizeof(payloadBuf_), "[%s] %s: %s", severity, unitStr, message);
    } else {
        snprintf(payloadBuf_, sizeof(payloadBuf_), "[%s] %s", severity, message);
    }

    bool success = mqtt_->publish(topicBuf_, payloadBuf_);

    if (success) {
        HAL_LOG_INFO("HA_PUB", "Alert published: %s", payloadBuf_);
    }

    return success;
}

} // namespace ha
} // namespace idryer

#endif // ESP32
