/**
 * @file link_integrations_manager.cpp
 * @brief Реализация оркестратора LINK-интеграций.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "link_integrations_manager.h"
#include "../../mqtt/mqtt_client.h"
#include "../../hal/hal_types.h"
#include <string.h>
#include <time.h>

namespace idryer {
namespace cloud {

namespace {

void isoTimestamp(char* buf, size_t bufSize)
{
    if (!buf || bufSize < 21) {
        if (buf && bufSize) buf[0] = '\0';
        return;
    }
    time_t now = time(nullptr);
    if (now < 1000000000) {
        snprintf(buf, bufSize, "1970-01-01T00:00:00Z");
        return;
    }
    struct tm tmv{};
    gmtime_r(&now, &tmv);
    strftime(buf, bufSize, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

} // namespace

// =============================================================================
// Конструктор / инициализация
// =============================================================================

LinkIntegrationsManager::LinkIntegrationsManager(idryer::MqttClient* mqtt,
                                                 LinkIntegrationsStore* store)
    : mqtt_(mqtt), store_(store), haPublisher_(haClient_.mqttClient())
{
}

void LinkIntegrationsManager::begin()
{
    if (!store_) {
        HAL_LOG_ERROR("LINK_MGR", "begin: store is null");
        return;
    }

    store_->begin();
    store_->loadHa(ha_);
    store_->loadBambu(bambu_);
    store_->loadMoonraker(moonraker_);
    store_->loadCommon(selection_);

    bambuClient_.setStateChangeCallback([this](BambuConnectionState /*s*/) {
        publishStatus();
    });

    moonrakerClient_.setStateChangeCallback([this](MoonrakerConnectionState /*s*/) {
        publishStatus();
    });

    haClient_.setStateChangeCallback([this](HaConnectionState s) {
        if (s == HaConnectionState::Connected && haDeviceId_[0] != '\0') {
            haPublisher_.publishDiscovery(haDeviceId_, haUnitsCount_, haHwVersion_, haFwVersion_);
        } else if (s != HaConnectionState::Connected) {
            haPublisher_.resetDiscoveryPublished();
        }
        publishStatus();
    });

    HAL_LOG_INFO("LINK_MGR", "begin: active=%s ha=%d bambu=%d moonraker=%d",
                 activeIntegrationToString(selection_.active),
                 (int)ha_.configured(), (int)bambu_.configured(),
                 (int)moonraker_.configured());

    // Полный дамп NVS-сохранённых настроек при старте.
    HAL_LOG_INFO("LINK_MGR", "─── NVS dump (boot) ───");
    HAL_LOG_INFO("LINK_MGR", "  HA:        enabled=%d host='%s' port=%u user=%s pass=%s",
                 ha_.enabled ? 1 : 0,
                 ha_.host[0] ? ha_.host : "<empty>",
                 ha_.port,
                 ha_.username[0] ? ha_.username : "<empty>",
                 ha_.password[0] ? "<set>" : "<empty>");
    HAL_LOG_INFO("LINK_MGR", "  Bambu:     enabled=%d ip='%s' serial=%s lan=%s",
                 bambu_.enabled ? 1 : 0,
                 bambu_.ip[0] ? bambu_.ip : "<empty>",
                 bambu_.serial[0] ? "<set>" : "<empty>",
                 bambu_.lanAccessCode[0] ? "<set>" : "<empty>");
    HAL_LOG_INFO("LINK_MGR", "  Moonraker: enabled=%d host='%s' port=%u ssl=%d apiKey=%s",
                 moonraker_.enabled ? 1 : 0,
                 moonraker_.host[0] ? moonraker_.host : "<empty>",
                 moonraker_.port,
                 moonraker_.ssl ? 1 : 0,
                 moonraker_.apiKey[0] ? "<set>" : "<empty>");
    HAL_LOG_INFO("LINK_MGR", "  Active:    %s", activeIntegrationToString(selection_.active));
    HAL_LOG_INFO("LINK_MGR", "─── end NVS dump ───");

    applyActiveIntegration();
    publishStatus();
}

// =============================================================================
// Обработчики команд
// =============================================================================

void LinkIntegrationsManager::handleLinkIntegrationCommand(JsonObjectConst data)
{
    if (!store_) return;

    const char* type = data["type"] | (const char*)nullptr;
    if (!type) {
        HAL_LOG_WARN("LINK_MGR", "link_integration: missing 'type'");
        return;
    }

    if (strcmp(type, "ha") == 0) {
        HaConfig fresh;
        if (!parseHa(data, fresh)) {
            HAL_LOG_WARN("LINK_MGR", "link_integration ha: parse failed");
            return;
        }
        ha_ = fresh;
        store_->saveHa(ha_);
        haLastError_[0] = '\0';

        if (selection_.active == ActiveIntegration::Ha) {
            haClient_.configure(ha_);
        }
    }
    else if (strcmp(type, "bambu") == 0) {
        BambuConfig fresh;
        if (!parseBambu(data, fresh)) {
            HAL_LOG_WARN("LINK_MGR", "link_integration bambu: parse failed");
            return;
        }
        bambu_ = fresh;
        store_->saveBambu(bambu_);
        bambuLastError_[0] = '\0';

        if (selection_.active == ActiveIntegration::Bambu) {
            bambuClient_.configure(bambu_);
        }
    }
    else if (strcmp(type, "moonraker") == 0) {
        MoonrakerConfig fresh;
        if (!parseMoonraker(data, fresh)) {
            HAL_LOG_WARN("LINK_MGR", "link_integration moonraker: parse failed");
            return;
        }
        moonraker_ = fresh;
        store_->saveMoonraker(moonraker_);
        moonrakerLastError_[0] = '\0';

        if (selection_.active == ActiveIntegration::Moonraker) {
            moonrakerClient_.configure(moonraker_);
        }
    }
    else {
        HAL_LOG_WARN("LINK_MGR", "link_integration: unknown type='%s'", type);
        return;
    }

    publishStatus();
}

void LinkIntegrationsManager::handleBambuApplyCommand(JsonObjectConst data)
{
    if (selection_.active != ActiveIntegration::Bambu) {
        HAL_LOG_INFO("LINK_MGR", "bambu_apply ignored: active=%s (not bambu)",
                     activeIntegrationToString(selection_.active));
        return;
    }
    if (!bambu_.configured()) {
        HAL_LOG_WARN("LINK_MGR", "bambu_apply ignored: bambu not configured");
        strncpy(bambuLastError_, "not configured", sizeof(bambuLastError_) - 1);
        bambuLastError_[sizeof(bambuLastError_) - 1] = '\0';
        publishStatus();
        return;
    }
    if (!bambu_.autoApplyOnTagDetect) {
        HAL_LOG_INFO("LINK_MGR", "bambu_apply ignored: autoApplyOnTagDetect=false");
        return;
    }

    BambuApplyPayload parsed;
    if (!parseBambuApply(data, parsed)) {
        HAL_LOG_WARN("LINK_MGR", "bambu_apply: parse failed");
        strncpy(bambuLastError_, "bambu_apply payload invalid",
                sizeof(bambuLastError_) - 1);
        bambuLastError_[sizeof(bambuLastError_) - 1] = '\0';
        publishStatus();
        return;
    }

    BambuApplyResult result = bambuClient_.applyFilament(parsed);

    isoTimestamp(bambuLastApplyAt_, sizeof(bambuLastApplyAt_));
    strncpy(bambuLastApplyResult_, result.success ? "ok" : "failed",
            sizeof(bambuLastApplyResult_) - 1);
    bambuLastApplyResult_[sizeof(bambuLastApplyResult_) - 1] = '\0';

    strncpy(bambuLastApplySpoolId_, parsed.spoolId, sizeof(bambuLastApplySpoolId_) - 1);
    bambuLastApplySpoolId_[sizeof(bambuLastApplySpoolId_) - 1] = '\0';

    bambuLastApplyAmsId_  = (parsed.amsId  == kBambuApplyAmsFromConfig)
                              ? bambu_.defaultAmsId  : parsed.amsId;
    bambuLastApplyTrayId_ = (parsed.trayId == kBambuApplyTrayFromConfig)
                              ? bambu_.defaultTrayId : parsed.trayId;
    bambuHasLastApply_ = true;

    if (result.success) {
        bambuLastError_[0] = '\0';
    } else {
        strncpy(bambuLastError_, result.errorMessage, sizeof(bambuLastError_) - 1);
        bambuLastError_[sizeof(bambuLastError_) - 1] = '\0';
        HAL_LOG_WARN("LINK_MGR", "bambu_apply failed: %s", result.errorMessage);
    }

    publishStatus();
}

void LinkIntegrationsManager::setActive(ActiveIntegration active)
{
    if (selection_.active == active) return;

    HAL_LOG_INFO("LINK_MGR", "setActive: %s -> %s",
                 activeIntegrationToString(selection_.active),
                 activeIntegrationToString(active));

    selection_.active = active;
    if (store_) store_->saveCommon(selection_);

    applyActiveIntegration();
    publishStatus();
}

void LinkIntegrationsManager::loop()
{
    bambuClient_.loop();
    moonrakerClient_.loop();
    haClient_.loop();

    if (mqtt_ && mqtt_->isConnected()
        && selection_.active != ActiveIntegration::None)
    {
        uint32_t now = millis();
        if (now - lastStatusPublishMs_ >= kPeriodicStatusIntervalMs) {
            publishStatus();
        }
    }
}

// =============================================================================
// Device info for HA discovery
// =============================================================================

void LinkIntegrationsManager::setDeviceInfo(const char* deviceId, uint8_t unitsCount,
                                             const char* hwVersion, const char* fwVersion)
{
    if (deviceId && deviceId[0]) {
        strncpy(haDeviceId_, deviceId, sizeof(haDeviceId_) - 1);
        haDeviceId_[sizeof(haDeviceId_) - 1] = '\0';
    }
    haUnitsCount_ = unitsCount > 0 ? unitsCount : 1;
    if (hwVersion && hwVersion[0]) {
        strncpy(haHwVersion_, hwVersion, sizeof(haHwVersion_) - 1);
        haHwVersion_[sizeof(haHwVersion_) - 1] = '\0';
    }
    if (fwVersion && fwVersion[0]) {
        strncpy(haFwVersion_, fwVersion, sizeof(haFwVersion_) - 1);
        haFwVersion_[sizeof(haFwVersion_) - 1] = '\0';
    }
}

// =============================================================================
// deviceType-aware
// =============================================================================

void LinkIntegrationsManager::setDeviceType(UartDeviceType deviceType)
{
    if (deviceType_ == deviceType) return;
    HAL_LOG_INFO("LINK_MGR", "setDeviceType: %u -> %u",
                 (unsigned)deviceType_, (unsigned)deviceType);
    deviceType_ = deviceType;
    applyActiveIntegration();
    publishStatus();
}

// =============================================================================
// Callbacks passthrough
// =============================================================================

void LinkIntegrationsManager::setChamberTargetCallback(MoonrakerClient::ChamberTargetCallback cb)
{
    moonrakerClient_.setChamberTargetCallback(std::move(cb));
}

void LinkIntegrationsManager::setMoonrakerStatusCallback(MoonrakerClient::StatusChangeCallback cb)
{
    moonrakerClient_.setStatusChangeCallback(std::move(cb));
}

void LinkIntegrationsManager::setVirtualChamberCallback(MoonrakerClient::VirtualChamberCallback cb)
{
    moonrakerClient_.setVirtualChamberCallback(std::move(cb));
}

void LinkIntegrationsManager::setBambuPrinterStatusCallback(BambuClient::PrinterStatusCallback cb)
{
    bambuClient_.setPrinterStatusCallback(std::move(cb));
}

// =============================================================================
// applyActiveIntegration
// =============================================================================

void LinkIntegrationsManager::applyActiveIntegration()
{
    BambuMode mode = BambuMode::Writer;
    if (deviceType_ == UartDeviceType::Heater
        || deviceType_ == UartDeviceType::IHeaterLink) {
        mode = BambuMode::Reader;
    }
    bambuClient_.setMode(mode);

    if (selection_.active == ActiveIntegration::Bambu) {
        bambuClient_.configure(bambu_);
    } else {
        bambuClient_.shutdown();
    }

    if (selection_.active == ActiveIntegration::Moonraker) {
        moonrakerClient_.configure(moonraker_);
    } else {
        moonrakerClient_.shutdown();
    }

    if (selection_.active == ActiveIntegration::Ha) {
        haClient_.configure(ha_);
    } else {
        haClient_.shutdown();
    }
}

// =============================================================================
// Публикация integrations/status
// =============================================================================

void LinkIntegrationsManager::publishStatus()
{
    if (!mqtt_) return;
    if (!mqtt_->isConnected()) {
        return;
    }

    DynamicJsonDocument doc(1024);

    doc["active"] = activeIntegrationToString(selection_.active);

    JsonObject haObj = doc.createNestedObject("ha");
    serializeHaSection(haObj);

    JsonObject bambuObj = doc.createNestedObject("bambu");
    serializeBambuSection(bambuObj);

    JsonObject moonrakerObj = doc.createNestedObject("moonraker");
    serializeMoonrakerSection(moonrakerObj);

    char ts[24];
    isoTimestamp(ts, sizeof(ts));
    doc["updatedAt"] = ts;

    mqtt_->publishIntegrationsStatus(doc);
    lastStatusPublishMs_ = millis();
    HAL_LOG_DEBUG("LINK_MGR", "integrations/status published");
}

// =============================================================================
// Сериализация секций
// =============================================================================

void LinkIntegrationsManager::serializeHaSection(JsonObject section) const
{
    section["configured"] = ha_.configured();
    section["enabled"]    = ha_.enabled;
    section["state"]      = integrationStateToString(computeHaState());
    if (ha_.host[0]) {
        section["host"]       = ha_.host;
        section["brokerPort"] = ha_.port;
    }
    section["authUsed"]   = haClient_.authConfigured();

    const char* runtimeErr = haClient_.lastError();
    if (runtimeErr && runtimeErr[0]) {
        section["lastError"] = runtimeErr;
    } else {
        section["lastError"] = haLastError_;
    }

    char ts[24];
    isoTimestamp(ts, sizeof(ts));
    section["updatedAt"] = ts;
}

void LinkIntegrationsManager::serializeBambuSection(JsonObject section) const
{
    section["configured"] = bambu_.configured();
    section["enabled"]    = bambu_.enabled;
    section["state"]      = integrationStateToString(computeBambuState());
    if (bambu_.ip[0]) {
        section["printerIp"]     = bambu_.ip;
        section["printerSerial"] = bambu_.serial;
    }
    section["lastError"]  = bambuLastError_;

    if (bambuHasLastApply_) {
        JsonObject la = section.createNestedObject("lastApply");
        la["at"]       = bambuLastApplyAt_;
        la["result"]   = bambuLastApplyResult_;
        la["spoolId"]  = bambuLastApplySpoolId_;
        la["amsId"]    = bambuLastApplyAmsId_;
        la["trayId"]   = bambuLastApplyTrayId_;
    }

    // Reader-mode: эмитим в MQTT только то, что реально используется потребителями.
    // currentFilament нужен порталу для отображения «сейчас на принтере PLA/PETG/...»,
    // а на устройстве iHeater для chamber-control используется напрямую через C++
    // callback (BambuPrinterStatus, см. auto_heat.cpp). Прочие Reader-mode поля
    // (printerState/progress/temps) убраны как неиспользуемые.
    if (bambuClient_.mode() == BambuMode::Reader) {
        const BambuPrinterStatus& ps = bambuClient_.printerStatus();
        if (ps.trayType[0]) {
            section["currentFilament"] = ps.trayType;
            if (ps.trayInfoIdx[0]) section["currentTrayInfoIdx"] = ps.trayInfoIdx;
        }
    }

    char ts[24];
    isoTimestamp(ts, sizeof(ts));
    section["updatedAt"] = ts;
}

void LinkIntegrationsManager::serializeMoonrakerSection(JsonObject section) const
{
    section["configured"]              = moonraker_.configured();
    section["enabled"]                 = moonraker_.enabled;
    section["state"]                   = integrationStateToString(computeMoonrakerState());
    if (moonraker_.host[0]) {
        section["host"] = moonraker_.host;
        section["port"] = moonraker_.port;
    }

    const MoonrakerStatus& ms = moonrakerClient_.status();
    section["virtualChamberAvailable"] = ms.virtualChamberAvailable;
    section["chamberHasSensor"]        = ms.chamberHasSensor;
    section["chamberTarget"]           = ms.chamberTarget;
    section["chamberTemperature"]      = ms.chamberTemperature;

    if (ms.printerState[0])  section["printerState"]         = ms.printerState;
    if (ms.progress > 0.0f)  section["progress"]             = ms.progress;
    if (ms.filename[0])      section["filename"]             = ms.filename;
    if (ms.nozzleTemp > 0.0f) {
        section["nozzleTemp"]   = ms.nozzleTemp;
        section["nozzleTarget"] = ms.nozzleTarget;
    }
    if (ms.bedTemp > 0.0f) {
        section["bedTemp"]    = ms.bedTemp;
        section["bedTarget"]  = ms.bedTarget;
    }
    if (ms.printDurationSeconds > 0) {
        section["printDurationSeconds"] = ms.printDurationSeconds;
    }

    const char* runtimeErr = moonrakerClient_.lastError();
    if (runtimeErr && runtimeErr[0]) {
        section["lastError"] = runtimeErr;
    } else {
        section["lastError"] = moonrakerLastError_;
    }

    char ts[24];
    isoTimestamp(ts, sizeof(ts));
    section["updatedAt"] = ts;
}

// =============================================================================
// Вычисление state
// =============================================================================

IntegrationState LinkIntegrationsManager::computeHaState() const
{
    if (selection_.active != ActiveIntegration::Ha) return IntegrationState::Disabled;
    if (!ha_.configured())                       return IntegrationState::ConfigMissing;

    switch (haClient_.state()) {
    case HaConnectionState::Connected:  return IntegrationState::Online;
    case HaConnectionState::Connecting: return IntegrationState::Connecting;
    case HaConnectionState::Error:      return IntegrationState::Error;
    case HaConnectionState::Disabled:
    case HaConnectionState::Idle:
    default:                            return IntegrationState::Idle;
    }
}

IntegrationState LinkIntegrationsManager::computeBambuState() const
{
    if (selection_.active != ActiveIntegration::Bambu) return IntegrationState::Disabled;
    if (!bambu_.configured())                       return IntegrationState::ConfigMissing;

    switch (bambuClient_.state()) {
    case BambuConnectionState::Connected:  return IntegrationState::Online;
    case BambuConnectionState::Connecting: return IntegrationState::Connecting;
    case BambuConnectionState::Error:      return IntegrationState::Error;
    case BambuConnectionState::Disabled:
    case BambuConnectionState::Idle:
    default:                               return IntegrationState::Idle;
    }
}

IntegrationState LinkIntegrationsManager::computeMoonrakerState() const
{
    if (selection_.active != ActiveIntegration::Moonraker) return IntegrationState::Disabled;
    if (!moonraker_.configured())                       return IntegrationState::ConfigMissing;

    switch (moonrakerClient_.state()) {
    case MoonrakerConnectionState::Connected:  return IntegrationState::Online;
    case MoonrakerConnectionState::Connecting: return IntegrationState::Connecting;
    case MoonrakerConnectionState::Error:      return IntegrationState::Error;
    case MoonrakerConnectionState::Disabled:
    case MoonrakerConnectionState::Idle:
    default:                                   return IntegrationState::Idle;
    }
}

// =============================================================================
// Парсинг payload
// =============================================================================

void LinkIntegrationsManager::copyField(JsonObjectConst data, const char* key,
                                        char* buf, size_t bufSize)
{
    if (!buf || bufSize == 0) return;
    buf[0] = '\0';
    const char* v = data[key] | (const char*)nullptr;
    if (!v) return;
    size_t len = strlen(v);
    if (len >= bufSize) len = bufSize - 1;
    memcpy(buf, v, len);
    buf[len] = '\0';
}

bool LinkIntegrationsManager::parseHa(JsonObjectConst data, HaConfig& out) const
{
    out = HaConfig{};

    out.enabled = data["enabled"] | false;
    copyField(data, "host",     out.host,            sizeof(out.host));
    copyField(data, "username", out.username,        sizeof(out.username));
    copyField(data, "password", out.password,        sizeof(out.password));
    copyField(data, "discoveryPrefix", out.discoveryPrefix, sizeof(out.discoveryPrefix));

    if (!out.host[0]) strncpy(out.host, "homeassistant.local", sizeof(out.host) - 1);
    if (!out.discoveryPrefix[0]) strncpy(out.discoveryPrefix, "homeassistant", sizeof(out.discoveryPrefix) - 1);

    uint32_t port = data["port"] | 1883u;
    if (port == 0 || port > 65535) port = 1883;
    out.port = static_cast<uint16_t>(port);

    return true;
}

bool LinkIntegrationsManager::parseBambu(JsonObjectConst data, BambuConfig& out) const
{
    out = BambuConfig{};

    out.enabled = data["enabled"] | false;
    copyField(data, "ip",            out.ip,            sizeof(out.ip));
    copyField(data, "serial",        out.serial,        sizeof(out.serial));
    copyField(data, "lanAccessCode", out.lanAccessCode, sizeof(out.lanAccessCode));

    out.defaultAmsId         = data["defaultAmsId"]         | 255;
    out.defaultTrayId        = data["defaultTrayId"]        | 254;
    out.autoApplyOnTagDetect = data["autoApplyOnTagDetect"] | true;

    if (out.enabled && !out.configured()) {
        HAL_LOG_WARN("LINK_MGR", "bambu: enabled but missing ip/serial/lanAccessCode");
    }
    return true;
}

bool LinkIntegrationsManager::parseBambuApply(JsonObjectConst data, BambuApplyPayload& out) const
{
    out = BambuApplyPayload{};

    int amsRaw  = data["amsId"]  | -1;
    int trayRaw = data["trayId"] | -1;
    if (amsRaw  >= 0 && amsRaw  <= 255) out.amsId  = static_cast<uint8_t>(amsRaw);
    if (trayRaw >= 0 && trayRaw <= 255) out.trayId = static_cast<uint8_t>(trayRaw);

    copyField(data, "trayType",     out.trayType,     sizeof(out.trayType));
    copyField(data, "colorHex",     out.colorHex,     sizeof(out.colorHex));
    copyField(data, "trayInfoIdx",  out.trayInfoIdx,  sizeof(out.trayInfoIdx));
    copyField(data, "settingId",    out.settingId,    sizeof(out.settingId));
    copyField(data, "spoolId",      out.spoolId,      sizeof(out.spoolId));
    copyField(data, "uid",          out.uid,          sizeof(out.uid));

    out.nozzleTempMin = data["nozzleTempMin"] | 0;
    out.nozzleTempMax = data["nozzleTempMax"] | 0;

    return out.valid();
}

bool LinkIntegrationsManager::parseMoonraker(JsonObjectConst data, MoonrakerConfig& out) const
{
    out = MoonrakerConfig{};

    out.enabled = data["enabled"] | false;
    copyField(data, "host",   out.host,   sizeof(out.host));
    copyField(data, "apiKey", out.apiKey, sizeof(out.apiKey));

    uint32_t port = data["port"] | 7125u;
    if (port == 0 || port > 65535) port = 7125;
    out.port = static_cast<uint16_t>(port);

    out.ssl             = data["ssl"]            | false;
    out.pollIntervalMs  = data["pollIntervalMs"] | 1000u;
    if (out.pollIntervalMs < 100) out.pollIntervalMs = 100;

    if (out.enabled && !out.configured()) {
        HAL_LOG_WARN("LINK_MGR", "moonraker: enabled but missing host");
    }
    return true;
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
