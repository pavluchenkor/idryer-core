/**
 * @file iDryer.cpp
 * @brief Implementation of iDryer::Link facade.
 *
 * Build target: ESP32 / Arduino. Conditionally compiled.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "iDryer.h"

#include "mqtt/mqtt_client.h"   // MQTT_CONFIG_CHUNK_SIZE
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ImprovWiFiLibrary.h>
#include <string.h>     // strcmp

#include "idryer_core.h"
#include "idryer_integrations.h"
#include "cloud/cloud_state_machine.h"
#include "local_access/local_access.h"
#include "local_access/device_publisher.h"

#ifndef IDRYER_API_BASE
#  error "IDRYER_API_BASE must be defined via build_flags (e.g. \"https://portal.idryer.org/api\")"
#endif

namespace iDryer {

namespace {

// Forward — определение ниже в этом же anonymous namespace (строка ~449).
// Нужно вверху чтобы Link::loop() мог его вызвать.
const char* unitModeString(UnitMode m);

// ──────────────────────────────────────────────────────────────────────
//  Internal Profile — generates info JSON from Config.
//  Hides IProfile from the public API.
// ──────────────────────────────────────────────────────────────────────

class FacadeProfile : public idryer::IProfile {
public:
    FacadeProfile(const Config& cfg, const idryer::ArduinoCredentialStore& credentials,
                  idryer::cloud::CloudStateMachine* cloud = nullptr)
        : cfg_(cfg), credentials_(credentials), cloud_(cloud) {}

    void onOnline() override {
        // No product-specific hardware to bring online — facade is generic.
    }

    void loop() override {
        // No periodic work owned by the profile itself.
    }

    void getConfig(JsonDocument& out) override {
        // TODO: declarative config (commands/set) is product-specific.
        // For now — return an empty object. Application code that needs
        // NVS-backed config can use the lower-level idryer_core API.
        out.to<JsonObject>();
    }

    bool applyConfig(int /*id*/, int /*val*/) override {
        // See getConfig() — no facade-level config. Return false so the
        // runtime knows the parameter wasn't handled.
        return false;
    }

    void buildInfoJson(char* buf, size_t len) const override {
        // info payload published (retained) to idryer/{serial}/info.
        // Shape is legacy-compatible — portal validator expects exactly this.
        // See iHeater-link/src/heater/HeaterProfile.cpp:52 for reference.
        idryer::DeviceIdentity id;
        const_cast<idryer::ArduinoCredentialStore&>(credentials_).load(id);

        StaticJsonDocument<1024> doc;
        doc["hardwareVersion"] = cfg_.hardwareVersion ? cfg_.hardwareVersion : "";
        doc["firmwareVersion"] = cfg_.firmwareVersion ? cfg_.firmwareVersion : "";
        doc["workTimeCounter"] = millis() / 1000u;
        doc["unitsCount"]      = cfg_.unitsCount;
        // For two-chip devices, use the mcuSerial from CloudStateMachine (set via
        // UART Hello). For one-ID devices (no cloud_ or no mcuSerial set),
        // fall back to identity_.serialNumber = DEVICE_... as before.
        const char* mcu = cloud_ ? cloud_->getMcuSerial() : nullptr;
        if (mcu && mcu[0] != '\0') {
            doc["mcuSerial"] = mcu;
        } else if (id.hasSerialNumber()) {
            doc["mcuSerial"] = id.serialNumber;
        }
        const char* mcuFw = cloud_ ? cloud_->getMcuFirmwareVersion() : nullptr;
        if (mcuFw && mcuFw[0] != '\0') {
            doc["mcuFirmwareVersion"] = mcuFw;
        }
        doc["deviceType"] = deviceTypeString(cfg_.deviceType);
        if (cfg_.model && cfg_.model[0] != '\0') {
            doc["model"] = cfg_.model;
        }

        // units[] with per-unit capabilities (legacy field names).
        JsonArray units = doc.createNestedArray("units");
        for (uint8_t i = 0; i < cfg_.unitsCount && i < MAX_UNITS; ++i) {
            JsonObject u = units.createNestedObject();
            u["unitId"] = i;        // integer per legacy

            JsonObject caps = u.createNestedObject("capabilities");
            caps["heater"]           = cfg_.hasHeater;
            caps["fan"]              = cfg_.hasFan;
            caps["servo"]            = false;     // not in Config
            caps["RhAirSensor"]      = cfg_.hasAirHumidity;
            caps["TempAirSensor"]    = cfg_.hasAirTemp;
            caps["TempHeaterSensor"] = cfg_.hasHeaterTemp;

            u.createNestedArray("scales");   // empty array
            u.createNestedArray("rfid");     // empty array
        }

        serializeJson(doc, buf, len);
    }

private:
    idryer::cloud::CloudStateMachine* cloud_ = nullptr;

    static const char* deviceTypeString(DeviceType t) {
        switch (t) {
            case DeviceType::Dryer:       return "dryer";
            case DeviceType::Heater:      return "heater";
            case DeviceType::StorageLink: return "storage_link";
            case DeviceType::IHeaterLink: return "iheater_link";
            case DeviceType::Unknown:     break;
        }
        return "unknown";
    }

    const Config& cfg_;
    const idryer::ArduinoCredentialStore& credentials_;
};

} // anonymous namespace

const char* deviceTypeToString(DeviceType t) {
    switch (t) {
        case DeviceType::Dryer:       return "dryer";
        case DeviceType::Heater:      return "heater";
        case DeviceType::StorageLink: return "storage_link";
        case DeviceType::IHeaterLink: return "iheater_link";
        case DeviceType::Unknown:     break;
    }
    return "unknown";
}

// ──────────────────────────────────────────────────────────────────────
//  Link::Impl — full SDK stack on stack (no heap).
// ──────────────────────────────────────────────────────────────────────

struct Link::Impl {
    explicit Impl(const Config& cfg)
        : cfg(cfg),
          api(&http, IDRYER_API_BASE),
          cloud(&wifi, &credentials, &api, &mqtt),
          pub(&mqtt, &local),
          intManager(&mqtt, &intStore),
          improv(&Serial),
          profile(this->cfg, credentials, &cloud),
          runtime(&cloud, &dispatcher, &profile, &mqtt) {}

    // Saved configuration (mutable — setUnitsCount() updates it at runtime).
    Config cfg;

    // Platform layer.
    idryer::ArduinoWifiStore       wifiStore;
    idryer::ArduinoWifiManager     wifi;
    idryer::ArduinoCredentialStore credentials;
    idryer::ArduinoHttpClient      http;

    // Cloud stack.
    idryer::cloud::HttpApi           api;
    idryer::MqttClient               mqtt;
    idryer::cloud::CloudStateMachine cloud;
    idryer::ActionDispatcher         dispatcher;

    // Local transport.
    idryer::LocalAccess     local;
    idryer::DevicePublisher pub;

    // Integrations.
    idryer::cloud::LinkIntegrationsStore   intStore;
    idryer::cloud::LinkIntegrationsManager intManager;

    // WiFi provisioning over Serial.
    ImprovWiFi improv;

    // Internal profile (generates info JSON from cfg).
    FacadeProfile profile;

    // Runtime — must be last; depends on cloud/dispatcher/profile/mqtt.
    idryer::IdryerRuntime runtime;

    // Command registry (для onCommand). Stack-array без heap.
    struct CommandEntry {
        char name[28];
        Link::CommandCallback cb;
    };
    static constexpr uint8_t MAX_CMDS = 12;
    CommandEntry commands[MAX_CMDS];
    uint8_t      commandsCount = 0;

    // Periodic task scheduler (для every/cancel). Stack-array.
    struct Task {
        uint32_t periodMs;
        uint32_t lastRunMs;
        Link::TaskCallback cb;
        bool active;
    };
    static constexpr uint8_t MAX_TASKS = 8;
    Task tasks[MAX_TASKS]{};

    // User callbacks.
    Link::IntegrationStatusCallback onIntegrationStatus;
    Link::ClaimPinCallback          onClaimPin;
    Link::DiagnosticCallback        onDiagnostic;
    Link::PublishHookCallback       onTelemetryPublish;
    Link::PublishHookCallback       onStatusPublish;

    // Boot state.
    bool logsEnabled = false;
    bool localStarted = false;       ///< mDNS+WS lazily started after WiFi connect

    // Auto-publish throttling (millis).
    uint32_t lastTelemetryMs = 0;
    uint32_t lastStatusMs    = 0;
    uint32_t lastHaStateMs   = 0;
    static constexpr uint32_t kHaStatePeriodMs = 5000;

    // sessionNum tracker: backend status.handler.ts requires sessionNum > 0
    // for active modes (DRYING/STORAGE/PROFILE). Increment on transition
    // IDLE/FAULT → active.
    uint32_t sessionNum[MAX_UNITS]   = {0, 0, 0, 0};
    UnitMode lastModeForSn[MAX_UNITS]= { UnitMode::Idle, UnitMode::Idle,
                                         UnitMode::Idle, UnitMode::Idle };

    // Device-wide remote-control gate. true → SDK отклоняет входящие команды
    // из MQTT/Local-WS и публикует event COMMAND_REJECTED с reason=ignore_external_cmd.
    // Source of truth — NVS/EEPROM продукта; продукт вызывает setIgnoreExternalCmd()
    // при загрузке и при изменении из локального меню.
    bool ignoreExternalCmd = false;
};

// ──────────────────────────────────────────────────────────────────────
//  Link — facade methods.
// ──────────────────────────────────────────────────────────────────────

Link::Link(const Config& cfg) {
    // Static local — живёт в .bss, нет heap allocation.
    // Конструктор приватного Impl доступен здесь т.к. мы внутри Link.
    static Impl s_impl(cfg);
    impl_ = &s_impl;

    for (uint8_t i = 0; i < MAX_UNITS; ++i) {
        telemetry.airTempC[i]       = 0.0f;
        telemetry.airHumidityPct[i] = 0.0f;
        telemetry.heaterTempC[i]    = 0.0f;
        telemetry.heaterPower01[i]  = 0.0f;
        telemetry.fanOn[i]          = false;

        status.mode[i]        = UnitMode::Idle;
        status.targetTempC[i] = 0.0f;
        status.durationS[i]   = 0;
        status.elapsedS[i]    = 0;
    }
}

Link::~Link() { impl_ = nullptr; }

bool Link::begin() {
    Serial.begin(115200);

#ifdef IDRYER_DEV_REPL
    // Dev mode: HAL logs go to Serial right away; product owns Serial input.
    idryer::hal::initArduinoHal(&Serial);
    impl_->logsEnabled = true;
#else
    // Production: Improv holds Serial until WiFi connects — keep HAL silent.
    idryer::hal::initArduinoHal(nullptr);

    impl_->improv.setDeviceInfo(
        ImprovTypes::ChipFamily::CF_ESP32_C3,
        "iDryer Link",
        impl_->cfg.firmwareVersion ? impl_->cfg.firmwareVersion : "1.0.0",
        "iDryer",
        "");
    impl_->improv.onImprovConnected([](const char* ssid, const char* password) {
        if (Link::s_currentImpl) {
            Link::s_currentImpl->wifiStore.save(ssid, password);
            Link::s_currentImpl->wifi.begin(ssid, password);
        }
    });
    Link::s_currentImpl = impl_;
#endif

    // Restore saved WiFi credentials if any.
    // WiFi.begin() called directly so the non-DEV_REPL loop (which returns early
    // before runtime.loop()) can observe WL_CONNECTED without cloud state machine.
    char ssid[64], pass[64];
    if (impl_->wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        impl_->wifi.begin(ssid, pass);
        WiFi.begin(ssid, pass);
    }

    // Serial number from MAC: `DEVICE_<12HEX_UPPERCASE>` (see %02X in
    // ArduinoCredentialStore::seedSerialFromMac, contract rules.serial_format).
    impl_->credentials.seedSerialFromMac();

    // mDNS + LAN WS.
    idryer::DeviceIdentity identity;
    impl_->credentials.load(identity);

    // mDNS / LAN WS server start lazily after WiFi connects (lwIP needs
    // network stack ready). See Link::loop() — `localStarted_` gate.
    impl_->local.setCommandSink([](void* ctx, const char* command, JsonObjectConst data) {
        static_cast<Link*>(ctx)->dispatchCommand(command, data);
    }, this);
    impl_->local.setTokenRefreshCallback([](void* ctx) {
        auto* self = static_cast<Link*>(ctx);
        idryer::DeviceIdentity id;
        self->impl_->credentials.load(id);
        self->impl_->local.updateToken(id.token);
    }, this);

    // Integrations.
    impl_->intStore.begin();
    if (identity.serialNumber[0] != '\0') {
        impl_->intManager.setHaClientId(identity.serialNumber);
        impl_->intManager.setDeviceInfo(identity.serialNumber,
                                        impl_->cfg.unitsCount,
                                        impl_->cfg.hardwareVersion,
                                        impl_->cfg.firmwareVersion);
        // Capabilities → HA Discovery публикует только реальные sensor entity.
        idryer::ha::HaCapabilities caps;
        caps.airTemp     = impl_->cfg.hasAirTemp;
        caps.airHumidity = impl_->cfg.hasAirHumidity;
        caps.heaterPower = impl_->cfg.hasHeater;
        caps.fan         = impl_->cfg.hasFan;
        caps.weight      = impl_->cfg.hasWeight;
        impl_->intManager.setHaCapabilities(caps);
    }
    // Map facade DeviceType → SDK UartDeviceType.
    switch (impl_->cfg.deviceType) {
        case DeviceType::Dryer:
            impl_->intManager.setDeviceType(idryer::UartDeviceType::Dryer);
            break;
        case DeviceType::Heater:
            impl_->intManager.setDeviceType(idryer::UartDeviceType::Heater);
            break;
        case DeviceType::StorageLink:
            impl_->intManager.setDeviceType(idryer::UartDeviceType::Link);
            break;
        case DeviceType::IHeaterLink:
            impl_->intManager.setDeviceType(idryer::UartDeviceType::IHeaterLink);
            break;
        case DeviceType::Unknown:
            break;
    }
    // Integration callbacks (chamber target / printer state) are wired by
    // the product, not the SDK — the SDK doesn't know about menu lookups
    // (filament type → menu.mat_petg, etc.). Use Link::integrationsManager()
    // accessor in main.cpp to subscribe.

    impl_->intManager.begin();

    // Runtime command handler — same dispatch as local-WS, single user callback.
    impl_->runtime.setCommandHandler([](void* ctx, const char* command, JsonObjectConst data) {
        static_cast<Link*>(ctx)->dispatchCommand(command, data);
    }, this);

    // Auto-claim for standalone devices — отключён: claim только по START_CLAIM от flasher.
    // impl_->cloud.setUnclaimedCallback([](void* ctx) {
    //     static_cast<Link::Impl*>(ctx)->cloud.requestClaim();
    // }, impl_);

    // Claim PIN: cloud → user callback (Serial, UI, etc).
    impl_->cloud.setClaimPinCallback([](const char* pin, uint32_t expires, void* ctx) {
        auto* self = static_cast<Link::Impl*>(ctx);
        if (self->onClaimPin) self->onClaimPin(pin, expires);
    }, impl_);
    impl_->cloud.setDiagnosticCallback([](const char* message, void* ctx) {
        auto* self = static_cast<Link::Impl*>(ctx);
        if (self->onDiagnostic) self->onDiagnostic(message);
    }, impl_);

    // Bring runtime online.
    impl_->runtime.begin();

    return true;
}

void Link::loop() {
#ifndef IDRYER_DEV_REPL
    // До WiFi: только Improv. runtime.loop() → cloud.loop() →
    // WiFi.scanNetworks (~5с) переполняет USB CDC FIFO и ломает Improv-RPC.
    if (!impl_->logsEnabled) {
        impl_->improv.handleSerial();
        if (WiFi.status() == WL_CONNECTED) {
            impl_->logsEnabled = true;
            idryer::hal::initArduinoHal(&Serial);
            Serial.println("[BOOT] WiFi ok, logs enabled");
            Serial.flush();
        }
        return;
    }

    // После WiFi: слушаем flasher-portal команды по Serial.
    {
        static char   s_serial_buf[64];
        static uint8_t s_serial_len = 0;
        while (Serial.available() > 0) {
            char c = (char)Serial.read();
            if (c == '\r' || c == '\n') {
                if (s_serial_len > 0) {
                    s_serial_buf[s_serial_len] = '\0';
                    s_serial_len = 0;
                    const char* cmd = s_serial_buf;
                    if (strcmp(cmd, "START_CLAIM") == 0 || strcmp(cmd, "claim") == 0) {
                        idryer::DeviceIdentity id;
                        impl_->credentials.load(id);
                        iDryer::ClaimRequestResult result = requestClaimDetailed();
                        switch (result) {
                            case iDryer::ClaimRequestResult::Started:
                                Serial.println("CLAIM_STARTED:OK");
                                break;
                            case iDryer::ClaimRequestResult::AlreadyClaimed:
                                Serial.printf("CLAIM_ALREADY:%s\n",
                                              id.hasSerialNumber() ? id.serialNumber : "?");
                                break;
                            case iDryer::ClaimRequestResult::StaleNvs:
                                Serial.printf("CLAIM_STALE_NVS:%s:%s\n",
                                              id.hasSerialNumber() ? id.serialNumber : "?",
                                              id.hasDeviceId() ? id.deviceId : "?");
                                break;
                            case iDryer::ClaimRequestResult::WaitingForMcuSerial:
                                Serial.println("CLAIM_STARTED:ERROR:WAITING_FOR_MCU_SERIAL");
                                break;
                            case iDryer::ClaimRequestResult::WifiNotConnected:
                                Serial.println("CLAIM_STARTED:ERROR:WIFI_NOT_CONNECTED");
                                break;
                            case iDryer::ClaimRequestResult::TokenWithheld:
                                Serial.println("CLAIM_STARTED:ERROR:TOKEN_WITHHELD");
                                break;
                            case iDryer::ClaimRequestResult::ProvisionFailed:
                                Serial.println("CLAIM_STARTED:ERROR:PROVISION_FAILED");
                                break;
                            case iDryer::ClaimRequestResult::RegisterFailed:
                            default:
                                Serial.println("CLAIM_STARTED:ERROR:REGISTER_FAILED");
                                break;
                        }
                        Serial.flush();
                    }
                }
            } else if (s_serial_len < sizeof(s_serial_buf) - 1) {
                s_serial_buf[s_serial_len++] = c;
            } else {
                s_serial_len = 0;  // overflow — сброс
            }
        }
    }
#endif

    impl_->runtime.loop();
    if (impl_->localStarted) impl_->local.loop();
    impl_->intManager.loop();

    // Lazy: mDNS/WS только после WiFi (lwIP requirement).
    if (!impl_->localStarted && WiFi.status() == WL_CONNECTED) {
        idryer::DeviceIdentity id;
        impl_->credentials.load(id);
        impl_->local.initMdns(id.serialNumber);
        impl_->local.begin(id.serialNumber, id.token);
        impl_->localStarted = true;
    }

    // Auto-publish on Config-defined intervals. Period == 0 means disabled
    // (used by products that don't have telemetry or status — e.g. Storage Link
    // does not publish status). Skipped when both transports are offline.
    const uint32_t now = millis();
    const bool anyTransport = impl_->pub.isMqttConnected() || impl_->pub.isLocalConnected();
    if (anyTransport) {
        if (impl_->cfg.telemetryPeriodMs > 0 &&
            now - impl_->lastTelemetryMs >= impl_->cfg.telemetryPeriodMs) {
            impl_->lastTelemetryMs = now;
            publishTelemetryNow();
        }
        if (impl_->cfg.statusPeriodMs > 0 &&
            now - impl_->lastStatusMs >= impl_->cfg.statusPeriodMs) {
            impl_->lastStatusMs = now;
            publishStatusNow();
        }
    }

    // Авто-публикация sensor state в HA. Шлёт ровно поля из HaCapabilities.
    // Безопасно вызывать всегда — внутри проверка connected/discovery published.
    // Управляющие entities (controls) — продукт публикует сам.
    if (now - impl_->lastHaStateMs >= Impl::kHaStatePeriodMs) {
        impl_->lastHaStateMs = now;
        const auto& cfg = impl_->cfg;
        for (uint8_t i = 0; i < cfg.unitsCount && i < MAX_UNITS; ++i) {
            const float temp     = telemetry.airTempC[i];
            const float hum      = telemetry.airHumidityPct[i];
            const int   powerPct = (int)(telemetry.heaterPower01[i] * 100.0f);
            const bool  fan      = telemetry.fanOn[i];
            impl_->intManager.publishHaUnitState(i, temp, hum, powerPct, fan);
        }
    }

    // Cooperative scheduler — продуктовые задачи зарегистрированные через every().
    // Защита от wrap millis() через signed-сравнение.
    for (uint8_t i = 0; i < Impl::MAX_TASKS; ++i) {
        auto& t = impl_->tasks[i];
        if (!t.active) continue;
        if ((int32_t)(now - t.lastRunMs) >= (int32_t)t.periodMs) {
            t.lastRunMs = now;
            t.cb();
        }
    }
}

namespace {

// "U1".."U4" — formatted unitId string, contract convention.
inline void formatUnitId(uint8_t i, char* buf /*[3]*/) {
    buf[0] = 'U';
    buf[1] = static_cast<char>('1' + i);
    buf[2] = '\0';
}

// "U1".."U4" → 0..3, иначе 0xFF (device-wide / unspecified).
uint8_t parseUnitId(const char* s) {
    if (!s) return 0xFF;
    if (s[0] == 'U' && s[1] >= '1' && s[1] <= '4' && s[2] == '\0') {
        return static_cast<uint8_t>(s[1] - '1');
    }
    return 0xFF;
}

// UnitMode → contract string. Mirrors yaml.enums.UartDryerMode + PortalUnitStatus.UNKNOWN.
const char* unitModeString(UnitMode m) {
    switch (m) {
        case UnitMode::Idle:           return "IDLE";
        case UnitMode::Drying:         return "DRYING";
        case UnitMode::Storage:        return "STORAGE";
        case UnitMode::Profile:        return "PROFILE";
        case UnitMode::Heating:        return "HEATING";
        case UnitMode::LightAnimation: return "LIGHT_ANIMATION";
        case UnitMode::Fault:          return "FAULT";
        case UnitMode::Unknown:        return "UNKNOWN";
    }
    return "UNKNOWN";
}

// EventKind → JSON `severity`. Канон CRIT/ERROR/WARN/INFO — единый словарь
// с RP2040 error bus и backend events.handler.
const char* eventSeverityString(EventKind k) {
    switch (k) {
        case EventKind::Info:     return "INFO";
        case EventKind::Warning:  return "WARN";
        case EventKind::Error:    return "ERROR";
        case EventKind::Critical: return "CRIT";
    }
    return "INFO";
}

} // anonymous namespace

void Link::publishTelemetryNow() {
    const Config& cfg = impl_->cfg;
    StaticJsonDocument<512> doc;

    JsonArray units = doc.createNestedArray("units");
    // CONTRACT (mqtt_contract.yaml §telemetry.units): итерируем только по
    // cfg.unitsCount — реально подтверждённых MCU юнитов. Нулевые данные
    // для несуществующих слотов в эфир не идут (см. setUnitsCount).
    for (uint8_t i = 0; i < cfg.unitsCount && i < MAX_UNITS; ++i) {
        char uid[3]; formatUnitId(i, uid);
        JsonObject u = units.createNestedObject();
        u["unitId"] = uid;

        // Only fields enabled in Config are emitted.
        // Float-поля: NAN ⇒ поле НЕ публикуется вообще (нет данных). Продукт
        // ставит NAN когда датчик не отвечает / не подключён. На проводе и в
        // случае cfg.has*=false, и в случае NAN — поле отсутствует. Frontend
        // обрабатывает одинаково: TelemetryRow скрывает cell.
        if (cfg.hasAirTemp) {
            float v = telemetry.airTempC[i];
            if (!isnan(v)) u["temperature"] = v;
        }
        if (cfg.hasAirHumidity) {
            float v = telemetry.airHumidityPct[i];
            if (!isnan(v)) u["humidity"] = v;
        }
        if (cfg.hasHeaterTemp) {
            float v = telemetry.heaterTempC[i];
            if (!isnan(v)) u["heaterTemp"] = v;
        }
        if (cfg.hasHeater) u["heaterPower"] = (int)roundf(telemetry.heaterPower01[i] * 100.0f);
        if (cfg.hasFan)    u["fanStatus"]   = telemetry.fanOn[i];
    }

    doc["rssi"]   = WiFi.RSSI();
    doc["uptime"] = millis() / 1000u;

    if (impl_->onTelemetryPublish) {
        impl_->onTelemetryPublish(doc.as<JsonObject>());
    }

    impl_->pub.publishTelemetry(doc);   // → MQTT + Local WS (timestamp added by SDK)
}

void Link::publishStatusNow() {
    const Config& cfg = impl_->cfg;
    StaticJsonDocument<512> doc;

    JsonArray units = doc.createNestedArray("units");
    // CONTRACT (mqtt_contract.yaml §status.units): то же ограничение — только
    // реально существующие юниты. Нулевые статусы несуществующих слотов не шлём.
    for (uint8_t i = 0; i < cfg.unitsCount && i < MAX_UNITS; ++i) {
        char uid[3]; formatUnitId(i, uid);
        JsonObject u = units.createNestedObject();
        u["unitId"] = uid;
        u["mode"]   = unitModeString(status.mode[i]);

        // sessionNum: backend requires > 0 for DRYING/STORAGE/PROFILE.
        // Increment on transition from non-active to active.
        // «Активный» режим = всё, что не Idle/Fault/Unknown. Phase 5: добавили
        // Heating, LightAnimation, Profile. Generic-проверка вместо whitelist,
        // чтобы новые mode'ы автоматически попадали в session-tracking без
        // правки SDK.
        auto isActiveMode = [](UnitMode m) {
            return m != UnitMode::Idle && m != UnitMode::Fault && m != UnitMode::Unknown;
        };
        const bool wasActive = isActiveMode(impl_->lastModeForSn[i]);
        const bool isActive  = isActiveMode(status.mode[i]);
        if (isActive && !wasActive) impl_->sessionNum[i]++;
        impl_->lastModeForSn[i] = status.mode[i];

        u["sessionNum"] = isActive ? impl_->sessionNum[i] : 0;

        // target: nested object per backend StatusPayload (mqtt-api.types.ts:154).
        // Emit only when meaningful (non-zero) to reduce noise on IDLE.
        if (status.targetTempC[i] > 0.0f || status.durationS[i] > 0) {
            JsonObject t = u.createNestedObject("target");
            t["temperature"] = status.targetTempC[i];
            if (status.durationS[i] > 0) {
                // backend expects MINUTES (matches DB Device.targetDurationMins).
                t["duration"] = status.durationS[i] / 60u;
            }
        }

        u["elapsedTime"] = status.elapsedS[i];
    }

    doc["uptime"] = millis() / 1000u;
    doc["rssi"]   = WiFi.RSSI();   // bonus: free signal info on every status
    doc["ignoreExternalCmd"] = impl_->ignoreExternalCmd;

    if (impl_->onStatusPublish) {
        impl_->onStatusPublish(doc.as<JsonObject>());
    }

    impl_->pub.publishStatus(doc);
}

void Link::raiseEvent(EventKind   severity,
                      const char* event,
                      const char* message,
                      uint8_t     unitId) {
    // Payload per mqtt_contract.yaml mqtt_only[suffix=events].
    // Required by Portal validator: severity, event, message, unitId.
    StaticJsonDocument<256> doc;
    doc["severity"] = eventSeverityString(severity);
    doc["event"]    = event   ? event   : "";
    doc["message"]  = message ? message : "";

    char uid[3];
    if (unitId < MAX_UNITS) {
        formatUnitId(unitId, uid);
        doc["unitId"] = uid;
    } else {
        doc["unitId"] = "DEVICE";   // device-wide convention
    }

    // timestamp is auto-injected by MqttClient::publishJson if absent.
    impl_->pub.publishEvent(doc);
}

bool Link::onCommand(const char* name, CommandCallback cb) {
    if (!name || !name[0] || !cb) return false;
    // Replace if name already registered.
    for (uint8_t i = 0; i < impl_->commandsCount; ++i) {
        if (strcmp(impl_->commands[i].name, name) == 0) {
            impl_->commands[i].cb = std::move(cb);
            return true;
        }
    }
    if (impl_->commandsCount >= Impl::MAX_CMDS) return false;
    auto& slot = impl_->commands[impl_->commandsCount++];
    strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.name[sizeof(slot.name) - 1] = '\0';
    slot.cb = std::move(cb);
    return true;
}

void Link::dispatchCommand(const char* command, JsonObjectConst data) {
    if (!command || !command[0]) return;

    // ─── Gate: ignoreExternalCmd ─────────────────────────────────────────
    // Защищает от внешних команд-ДЕЙСТВИЙ (запуск нагрева, включение ленты,
    // запись RFID и т.п.). Не блокирует read/config: пользователь должен
    // иметь возможность читать состояние и менять параметры даже когда
    // действия запрещены.
    //
    // Whitelist (всегда проходят):
    //   set, get_config, ping, link_integration,
    //   firmware_update_announce, firmware_check_update_response
    // Всё остальное (invoke, drying/storage/profile/stop, bambu_apply,
    // write_rfid, неизвестные) — блокируется и публикуется COMMAND_REJECTED.
    //
    // firmware_update_* — критическая security-инфраструктура (Phase 6).
    // Auto-update не должен блокироваться пользовательским гейтом, иначе
    // security-patches не доедут до устройств с ign_ext_cmd=true. Защита
    // OTA — это право push'а в admin/firmware/push (роль SUPERUSER в
    // backend), не флаг в меню устройства.
    if (impl_->ignoreExternalCmd) {
        const bool isExempt =
            (strcmp(command, "set") == 0) ||
            (strcmp(command, "get_config") == 0) ||
            (strcmp(command, "ping") == 0) ||
            (strcmp(command, "link_integration") == 0) ||
            (strcmp(command, "firmware_update_announce") == 0) ||
            (strcmp(command, "firmware_check_update_response") == 0);
        if (!isExempt) {
            StaticJsonDocument<256> doc;
            doc["severity"] = eventSeverityString(EventKind::Warning);
            doc["event"]    = "COMMAND_REJECTED";
            doc["message"]  = command;                       // имя отклонённой команды (human-readable)
            doc["unitId"]   = "DEVICE";                      // device-wide
            doc["reason"]   = "ignore_external_cmd";         // machine-readable
            if (data && data["commandId"].is<const char*>()) {
                doc["commandId"] = data["commandId"].as<const char*>();
            }
            impl_->pub.publishEvent(doc);
            HAL_LOG_WARN("LINK", "rejected '%s' (ignore_external_cmd=true)", command);
            return;
        }
        HAL_LOG_INFO("LINK", "passing '%s' through gate (read/config)", command);
    }

    // ─── Built-in side-effects (always run) ──────────────────────────────
    // Эти команды обрабатывает либа сама. Продукт может ДОПОЛНИТЕЛЬНО
    // подписаться через onCommand(name, ...) — будет вызван post-hook'ом,
    // например для menu-toggle sync после link_integration.
    bool builtinHandled = false;
    if (strcmp(command, "link_integration") == 0) {
        impl_->intManager.handleLinkIntegrationCommand(data);
        builtinHandled = true;
    } else if (strcmp(command, "bambu_apply") == 0) {
        impl_->intManager.handleBambuApplyCommand(data);
        builtinHandled = true;
    } else if (strcmp(command, "ping") == 0) {
        // Time-sync делает runtime через `timestamp` в payload — здесь no-op.
        builtinHandled = true;
    }

    // ─── Registry — продуктовые имена через onCommand(name, cb) ───────────
    for (uint8_t i = 0; i < impl_->commandsCount; ++i) {
        if (strcmp(impl_->commands[i].name, command) == 0) {
            impl_->commands[i].cb(data);
            return;
        }
    }

    if (!builtinHandled) {
        HAL_LOG_WARN("LINK", "unhandled command: %s", command);
    }
}

void Link::onClaimPin(ClaimPinCallback cb) {
    impl_->onClaimPin = std::move(cb);
}

void Link::onDiagnostic(DiagnosticCallback cb) {
    impl_->onDiagnostic = cb;
}

void Link::onTelemetryPublish(PublishHookCallback cb) {
    impl_->onTelemetryPublish = std::move(cb);
}

void Link::onStatusPublish(PublishHookCallback cb) {
    impl_->onStatusPublish = std::move(cb);
}

Link::TaskHandle Link::every(uint32_t periodMs, TaskCallback cb) {
    if (!cb || periodMs == 0) return 0xFF;
    for (uint8_t i = 0; i < Impl::MAX_TASKS; ++i) {
        if (!impl_->tasks[i].active) {
            impl_->tasks[i].periodMs  = periodMs;
            impl_->tasks[i].lastRunMs = millis();
            impl_->tasks[i].cb        = std::move(cb);
            impl_->tasks[i].active    = true;
            return i;
        }
    }
    return 0xFF;  // overflow
}

void Link::cancel(TaskHandle handle) {
    if (handle >= Impl::MAX_TASKS) return;
    impl_->tasks[handle].active = false;
    impl_->tasks[handle].cb     = nullptr;
}

void Link::onIntegrationStatus(IntegrationStatusCallback cb) {
    impl_->onIntegrationStatus = std::move(cb);
    // NOTE: LinkIntegrationsManager currently has no public state-change hook
    // (computeXxxState() is private). For now the callback is stored but never
    // invoked. To complete this — either add `setStateChangeCallback(...)` to
    // LinkIntegrationsManager, or poll `getActive()` + per-client status in
    // loop() and dispatch on diff. Decision deferred to a follow-up step.
}

bool Link::isOnline() const {
    return impl_->cloud.isOnline();
}

void Link::seedWifiCredentialsIfEmpty(const char* ssid, const char* password) {
    if (!ssid || !password) return;
    char curSsid[64], curPass[64];
    if (impl_->wifiStore.load(curSsid, sizeof(curSsid), curPass, sizeof(curPass))) {
        return;   // NVS already has credentials — don't overwrite
    }
    impl_->wifiStore.save(ssid, password);
}

void Link::setWifiCredentials(const char* ssid, const char* password) {
    if (!ssid || !password) return;
    impl_->wifiStore.save(ssid, password);
}

bool Link::requestClaim() {
    return impl_->cloud.requestClaim();
}

iDryer::ClaimRequestResult Link::requestClaimDetailed() {
    return impl_->cloud.requestClaimDetailed();
}

idryer::cloud::LinkIntegrationsManager* Link::integrationsManager() {
    return &impl_->intManager;
}

idryer::ha::HaBuilder& Link::ha() {
    return impl_->intManager.haBuilder();
}

idryer::MqttClient* Link::mqttClient() {
    return &impl_->mqtt;
}

idryer::DevicePublisher* Link::devicePublisher() {
    return &impl_->pub;
}

idryer::IdryerRuntime* Link::runtime() {
    return &impl_->runtime;
}

void Link::setUnitsCount(uint8_t n) {
    if (n < 1 || n > MAX_UNITS) return;
    // CONTRACT (mqtt_contract.yaml §units): MCU Hello — авторитетный источник
    // числа юнитов; вызывается из onHello(). Config.unitsCount в firmware должен
    // совпадать с реальным числом физических слотов, а не быть потолком MAX.
    impl_->cfg.unitsCount = n;
}

void Link::setIgnoreExternalCmd(bool flag) {
    if (impl_->ignoreExternalCmd == flag) return;
    impl_->ignoreExternalCmd = flag;
    // Сразу пубим status, чтобы портал быстро увидел изменение
    // (а не ждал следующего periodic-снимка).
    publishStatusNow();
}

bool Link::isIgnoreExternalCmd() const {
    return impl_->ignoreExternalCmd;
}

void Link::publishInfoNow() {
    char infoBuf[1024];
    impl_->profile.buildInfoJson(infoBuf, sizeof(infoBuf));
    impl_->pub.publishInfo(infoBuf);
}

void Link::eraseClaimAndRestart() {
    impl_->credentials.clear();
    delay(200);
    ESP.restart();
}

void Link::setWaitForMcuSerial(bool wait) {
    impl_->cloud.setWaitForMcuSerial(wait);
}

iDryer::McuSerialResult Link::setMcuSerial(const char* mcuSerial) {
    return impl_->cloud.setMcuSerial(mcuSerial);
}

void Link::setMcuFirmwareVersion(uint32_t fwVersion) {
    impl_->cloud.setMcuFirmwareVersion(fwVersion);
}

const char* Link::mcuSerial() const {
    return impl_->cloud.getMcuSerial();
}

const char* Link::mqttKey() const {
    return impl_->cloud.getMqttKey();
}

const char* Link::serial() const {
    return impl_->cloud.getIdentity().serialNumber;
}

// Definition of the static member (declared in iDryer.h).
Link::Impl* Link::s_currentImpl = nullptr;

} // namespace iDryer

#endif // ESP32 || ESP_PLATFORM
