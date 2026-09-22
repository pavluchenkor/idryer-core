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
    : mqtt_(mqtt), store_(store)
{
#if IDRYER_WITH_HA
    // Входящие с HA-брокера — генератору сущностей (команды и уборка).
    haClient_.mqttClient()->setMessageCallback(
        [](void* ctx, const char* topic, const char* payload) {
            auto* mgr = static_cast<LinkIntegrationsManager*>(ctx);
            if (mgr->haCard_) mgr->haCard_->handleIncoming(topic, payload);
        }, this);
#endif
}

// Интеграция собрана в образ? Чего нет — того нет: ни клиента, ни настройки,
// ни объявления в integrations/status.
bool LinkIntegrationsManager::isSupported(ActiveIntegration kind)
{
    switch (kind) {
    case ActiveIntegration::Ha:        return IDRYER_WITH_HA;
    case ActiveIntegration::Bambu:     return IDRYER_WITH_BAMBU;
    case ActiveIntegration::Moonraker: return IDRYER_WITH_MOONRAKER;
    default:                           return true;   // None выбрать можно всегда
    }
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

#if IDRYER_WITH_BAMBU
    bambuClient_.setStateChangeCallback([](void* ctx, BambuConnectionState) {
        static_cast<LinkIntegrationsManager*>(ctx)->publishStatus();
    }, this);
#endif

#if IDRYER_WITH_MOONRAKER
    moonrakerClient_.setStateChangeCallback([](void* ctx, MoonrakerConnectionState) {
        static_cast<LinkIntegrationsManager*>(ctx)->publishStatus();
    }, this);
#endif

#if IDRYER_WITH_HA
    haClient_.setStateChangeCallback([](void* ctx, HaConnectionState s) {
        auto* mgr = static_cast<LinkIntegrationsManager*>(ctx);
        if (mgr->haCard_) {
            if (s == HaConnectionState::Connected) mgr->haCard_->onConnected();
            else                                   mgr->haCard_->onDisconnected();
        }
        mgr->publishStatus();
    }, this);
#endif

    // Активная из NVS может быть от прошивки, где эта интеграция ещё собиралась.
    if (!isSupported(selection_.active)) {
        HAL_LOG_WARN("LINK_MGR", "active=%s не собрана в этой прошивке — сбрасываю",
                     activeIntegrationToString(selection_.active));
        selection_.active = ActiveIntegration::None;
        store_->saveCommon(selection_);
    }

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
    HAL_LOG_INFO("LINK_MGR", "  Собраны:   ha=%d bambu=%d moonraker=%d",
                 IDRYER_WITH_HA, IDRYER_WITH_BAMBU, IDRYER_WITH_MOONRAKER);
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

    ActiveIntegration kind = ActiveIntegration::None;
    if (activeIntegrationFromString(type, kind) && !isSupported(kind)) {
        HAL_LOG_WARN("LINK_MGR", "link_integration %s: интеграция не собрана в прошивке", type);
        return;
    }

#if IDRYER_WITH_HA
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
    else
#endif
#if IDRYER_WITH_BAMBU
    if (strcmp(type, "bambu") == 0) {
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
    else
#endif
#if IDRYER_WITH_MOONRAKER
    if (strcmp(type, "moonraker") == 0) {
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
    else
#endif
    {
        HAL_LOG_WARN("LINK_MGR", "link_integration: unknown type='%s'", type);
        return;
    }

    publishStatus();
}

void LinkIntegrationsManager::handleBambuApplyCommand(JsonObjectConst data)
{
#if !IDRYER_WITH_BAMBU
    (void)data;
    HAL_LOG_WARN("LINK_MGR", "bambu_apply: интеграция не собрана в прошивке");
    return;
#else
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
#endif
}

void LinkIntegrationsManager::setActive(ActiveIntegration active)
{
    if (!isSupported(active)) {
        HAL_LOG_WARN("LINK_MGR", "setActive %s: интеграция не собрана в прошивке",
                     activeIntegrationToString(active));
        return;
    }
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
#if IDRYER_WITH_BAMBU
    bambuClient_.loop();
#endif
#if IDRYER_WITH_MOONRAKER
    moonrakerClient_.loop();
#endif
#if IDRYER_WITH_HA
    haClient_.loop();
#endif

    // integrations/status — событийный (изменение состояния/конфига интеграции
    // публикует сразу; retained + QoS 1 хранят снапшот для портала). Здесь
    // только дошив отложенной публикации, если событие случилось до коннекта.
    if (statusPublishPending_ && mqtt_ && mqtt_->isConnected()) {
        publishStatus();
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

void LinkIntegrationsManager::setChamberTargetCallback(
    MoonrakerClient::ChamberTargetCallback::FnPtr fn, void* ctx)
{
#if IDRYER_WITH_MOONRAKER
    moonrakerClient_.setChamberTargetCallback(fn, ctx);
#else
    (void)fn; (void)ctx;
#endif
}

void LinkIntegrationsManager::setMoonrakerStatusCallback(
    MoonrakerClient::StatusChangeCallback::FnPtr fn, void* ctx)
{
#if IDRYER_WITH_MOONRAKER
    moonrakerClient_.setStatusChangeCallback(fn, ctx);
#else
    (void)fn; (void)ctx;
#endif
}

void LinkIntegrationsManager::setVirtualChamberCallback(
    MoonrakerClient::VirtualChamberCallback::FnPtr fn, void* ctx)
{
#if IDRYER_WITH_MOONRAKER
    moonrakerClient_.setVirtualChamberCallback(fn, ctx);
#else
    (void)fn; (void)ctx;
#endif
}

void LinkIntegrationsManager::setBambuPrinterStatusCallback(
    BambuClient::PrinterStatusCallback::FnPtr fn, void* ctx)
{
#if IDRYER_WITH_BAMBU
    bambuClient_.setPrinterStatusCallback(fn, ctx);
#else
    (void)fn; (void)ctx;
#endif
}

// =============================================================================
// applyActiveIntegration
// =============================================================================

void LinkIntegrationsManager::applyActiveIntegration()
{
#if IDRYER_WITH_BAMBU
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
#endif

#if IDRYER_WITH_MOONRAKER
    if (selection_.active == ActiveIntegration::Moonraker) {
        moonrakerClient_.configure(moonraker_);
    } else {
        moonrakerClient_.shutdown();
    }
#endif

#if IDRYER_WITH_HA
    if (selection_.active == ActiveIntegration::Ha) {
        haClient_.configure(ha_);
    } else {
        haClient_.shutdown();
    }
#endif
}

// =============================================================================
// Публикация integrations/status
// =============================================================================

void LinkIntegrationsManager::publishStatus()
{
    if (!mqtt_) return;
    if (!mqtt_->isConnected()) {
        // Событие случилось до коннекта (например, статус интеграций при
        // загрузке) — отложим: loop() опубликует, как только MQTT поднимется.
        statusPublishPending_ = true;
        return;
    }

    DynamicJsonDocument doc(1024);

    doc["active"] = activeIntegrationToString(selection_.active);

    // Что прибор вообще умеет: портал и приложение рисуют только эти интеграции.
    JsonArray supported = doc.createNestedArray("supported");
#if IDRYER_WITH_HA
    supported.add("ha");
    JsonObject haObj = doc.createNestedObject("ha");
    serializeHaSection(haObj);
#endif
#if IDRYER_WITH_BAMBU
    supported.add("bambu");
    JsonObject bambuObj = doc.createNestedObject("bambu");
    serializeBambuSection(bambuObj);
#endif
#if IDRYER_WITH_MOONRAKER
    supported.add("moonraker");
    JsonObject moonrakerObj = doc.createNestedObject("moonraker");
    serializeMoonrakerSection(moonrakerObj);
#endif

    char ts[24];
    isoTimestamp(ts, sizeof(ts));
    doc["updatedAt"] = ts;

    mqtt_->publishIntegrationsStatus(doc);
    statusPublishPending_ = false;
    HAL_LOG_DEBUG("LINK_MGR", "integrations/status published");
}

// =============================================================================
// Сериализация секций
// =============================================================================

void LinkIntegrationsManager::serializeHaSection(JsonObject section) const
{
#if IDRYER_WITH_HA
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
#else
    (void)section;
#endif
}

void LinkIntegrationsManager::serializeBambuSection(JsonObject section) const
{
#if IDRYER_WITH_BAMBU
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
#else
    (void)section;
#endif
}

void LinkIntegrationsManager::serializeMoonrakerSection(JsonObject section) const
{
#if IDRYER_WITH_MOONRAKER
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
#else
    (void)section;
#endif
}

// =============================================================================
// Вычисление state
// =============================================================================

IntegrationState LinkIntegrationsManager::computeHaState() const
{
#if IDRYER_WITH_HA
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
#else
    return IntegrationState::Disabled;
#endif
}

IntegrationState LinkIntegrationsManager::computeBambuState() const
{
#if IDRYER_WITH_BAMBU
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
#else
    return IntegrationState::Disabled;
#endif
}

IntegrationState LinkIntegrationsManager::computeMoonrakerState() const
{
#if IDRYER_WITH_MOONRAKER
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
#else
    return IntegrationState::Disabled;
#endif
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
