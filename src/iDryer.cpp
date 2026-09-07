/**
 * @file iDryer.cpp
 * @brief Implementation of iDryer::Link facade.
 *
 * Build target: ESP32 / Arduino. Conditionally compiled.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "iDryer.h"

#include "mqtt/mqtt_client.h"   // MQTT_CONFIG_CHUNK_SIZE
#include "work_time_tracker.h"  // накопительный workTimeCounter
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
#include "core/error_process.h"
#include "core/error_post.h"
#include "core/error_table.h"
#include <Preferences.h>
#include <esp_system.h>

#ifndef IDRYER_API_BASE
#  error "IDRYER_API_BASE must be defined via build_flags (e.g. \"https://portal.idryer.org/api\")"
#endif

namespace iDryer {

Link* Link::s_selfForErrors = nullptr;

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
        // workTimeCounter = наработка УСТРОЙСТВА. Для двухчипа это счётчик
        // RP2040 (из UART Hello) — он живёт с сушилкой и переживает замену
        // ESP-модуля. Для одночипа платы нет — берём ESP WorkTimeTracker.
        const uint32_t mcuWtc = cloud_ ? cloud_->getMcuWorkTimeCounter() : 0;
        doc["workTimeCounter"] = (mcuWtc > 0)
            ? mcuWtc
            : idryer::WorkTimeTracker::instance().total();
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
        const char* mcuHw = cloud_ ? cloud_->getMcuHardwareVersion() : nullptr;
        if (mcuHw && mcuHw[0] != '\0') {
            doc["mcuHardwareVersion"] = mcuHw;
        }
        doc["deviceType"] = deviceTypeString(cfg_.deviceType);
        if (cfg_.model && cfg_.model[0] != '\0') {
            doc["model"] = cfg_.model;
        }

        // units[] with per-unit capabilities (canonical vocabulary json_keys).
        JsonArray units = doc.createNestedArray("units");
        for (uint8_t i = 0; i < cfg_.unitsCount && i < MAX_UNITS; ++i) {
            JsonObject u = units.createNestedObject();
            u["unitId"] = i;        // integer per legacy

            // Ключи = json_key из capability_vocabulary (mqtt_contract.yaml).
            // Legacy-имена (RhAirSensor/TempAirSensor/TempHeaterSensor) убраны:
            // портал capabilities не валидирует, потребители читают оба нейминга.
            JsonObject caps = u.createNestedObject("capabilities");
            caps["heater"]       = cfg_.hasHeater;
            caps["fan"]          = cfg_.hasFan;
            caps["servo"]        = cfg_.hasServo;
            caps["led"]          = cfg_.hasLed;
            caps["weight"]       = cfg_.hasWeight;
            caps["rfid"]         = cfg_.hasRfid;
            caps["air_temp"]     = cfg_.hasAirTemp;
            caps["air_humidity"] = cfg_.hasAirHumidity;
            caps["heater_temp"]  = cfg_.hasHeaterTemp;

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
    Link::ClaimCompleteCallback     onClaimComplete;
    Link::DiagnosticCallback        onDiagnostic;
    Link::PublishHookCallback       onTelemetryPublish;
    Link::PublishHookCallback       onStatusPublish;

    // Boot state.
    bool logsEnabled = false;
    bool localStarted = false;       ///< mDNS+WS lazily started after WiFi connect

    // Entity manifest карточки (топик card, retained).
    idryer::CardBuilder card;
    bool cardPublished = false;      ///< опубликован ли манифест в этом коннекте

    // Auto-publish throttling (millis).
    uint32_t lastTelemetryMs = 0;
    uint32_t lastStatusMs    = 0;
    uint32_t lastHaStateMs   = 0;
    static constexpr uint32_t kHaStatePeriodMs = 5000;

    // sessionNum tracker: backend status.handler.ts requires sessionNum > 0
    // for active modes (DRYING/STORAGE/PROFILE). Increment on transition
    // IDLE/FAULT → active.
    // Снапшот значимых полей последнего опубликованного status — для
    // событийной публикации (loop сравнивает и шлёт сразу при изменении).
    UnitMode lastPubMode[MAX_UNITS]      = { UnitMode::Idle, UnitMode::Idle,
                                             UnitMode::Idle, UnitMode::Idle };
    float    lastPubTargetC[MAX_UNITS]   = {0, 0, 0, 0};
    uint32_t lastPubDurationS[MAX_UNITS] = {0, 0, 0, 0};

    // Снапшот булевых полей последней опубликованной телеметрии (fan/servo) —
    // смена состояния публикует телеметрию сразу, периодика остаётся сверкой.
    bool lastPubFanOn[MAX_UNITS]     = {false, false, false, false};
    bool lastPubServoOpen[MAX_UNITS] = {false, false, false, false};

    uint32_t sessionNum[MAX_UNITS]   = {0, 0, 0, 0};
    UnitMode lastModeForSn[MAX_UNITS]= { UnitMode::Idle, UnitMode::Idle,
                                         UnitMode::Idle, UnitMode::Idle };

    // Device-wide remote-control gate. true → SDK отклоняет входящие команды
    // из MQTT/Local-WS и публикует event COMMAND_REJECTED с reason=ignore_external_cmd.
    // Source of truth — NVS/EEPROM продукта; продукт вызывает setIgnoreExternalCmd()
    // при загрузке и при изменении из локального меню.
    bool ignoreExternalCmd = false;

    // Origin текущей команды: true — пришла с локального WS (LAN). Ставится в
    // dispatchCommand перед вызовом product-хендлера, читается им через
    // Link::currentCommandFromLocal() (чтобы не менять сигнатуру всех onCommand).
    // На ESP dispatch синхронный и однопоточный — гонок нет.
    bool currentCmdFromLocal = false;

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

    // Accumulated work-time counter — читаем из NVS до того как продукт
    // что-либо запустит. Любой ребут (включая OTA) теперь сохраняет общее
    // время работы устройства. См. work_time_tracker.h.
    idryer::WorkTimeTracker::instance().begin();

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
    // Свой connect для Improv вместо штатного tryConnectToWifi: тот сбрасывает
    // WiFi только если уже подключён, поэтому после нескольких попыток
    // (неверный→неверный→верный) драйвер ESP32-C3 стартует поверх мусорного
    // состояния и верный пароль "порой" не проходит. Полный сброс перед каждой
    // попыткой, до 15 c, ранний выход при явной ошибке.
    impl_->improv.setCustomConnectWiFi([](const char* ssid, const char* password) -> bool {
        WiFi.disconnect(true);
        delay(200);
        WiFi.begin(ssid, password);
        const uint32_t startMs = millis();
        while (millis() - startMs < 15000) {
            const wl_status_t st = WiFi.status();
            if (st == WL_CONNECTED) return true;
            if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) break;
            delay(200);
        }
        WiFi.disconnect(true);
        return false;
    });
    Link::s_currentImpl = impl_;
#endif

    // Pre-warm esp_wifi_init() here, before any UI/LVGL allocation on
    // products with a display (touch): esp_wifi_init needs a large
    // contiguous heap block, and on a fresh device (no stored credentials
    // below) the driver would otherwise only init later from the cloud
    // state machine's reconnect loop, by which point LVGL has fragmented
    // the heap enough that esp_wifi_init fails permanently (observed on
    // idryer-touch: free_heap=25KB but max_alloc=7.6KB at that point).
    // No-op for products without a display — just runs a bit earlier.
    impl_->wifi.begin(nullptr, nullptr);

    // Restore saved WiFi credentials if any.
    // WiFi.begin() called directly so the non-DEV_REPL loop (which returns early
    // before runtime.loop()) can observe WL_CONNECTED without cloud state machine.
    char ssid[64], pass[64];
    const bool haveWifiCreds =
        impl_->wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass));
    if (haveWifiCreds) {
        impl_->wifi.begin(ssid, pass);
        WiFi.begin(ssid, pass);
    }

    // Второй источник кредов рядом с Improv — по воздуху, из мобильного
    // приложения. Поднимается сам, когда сети нет: на чистом устройстве сразу,
    // с сохранёнными кредами — после таймаута подключения.
    idryer::EspTouchProvisioner::instance().begin(
        [](void* ctx, const char* s, const char* p) {
            static_cast<Link*>(ctx)->setWifiCredentials(s, p);
        },
        this, haveWifiCreds);

    // Serial number from MAC: `DEVICE_<12HEX_UPPERCASE>` (see %02X in
    // ArduinoCredentialStore::seedSerialFromMac, contract rules.serial_format).
    impl_->credentials.seedSerialFromMac();

    // mDNS + LAN WS.
    idryer::DeviceIdentity identity;
    impl_->credentials.load(identity);

    // mDNS / LAN WS server start lazily after WiFi connects (lwIP needs
    // network stack ready). See Link::loop() — `localStarted_` gate.
    impl_->local.setCommandSink([](void* ctx, const char* command, JsonObjectConst data) {
        // Локальный WS (LAN) — не гейтится ignoreExternalCmd.
        static_cast<Link*>(ctx)->dispatchCommand(command, data, /*fromLocal=*/true);
    }, this);
    impl_->local.setTokenRefreshCallback([](void* ctx) {
        auto* self = static_cast<Link*>(ctx);
        idryer::DeviceIdentity id;
        self->impl_->credentials.load(id);
        self->impl_->local.updateToken(id.token);
    }, this);
    // binding-v3: приложение подало токен привязки по локальному WS → в cloud.
    impl_->local.setPairingTokenCallback([](void* ctx, const char* token) {
        static_cast<Link*>(ctx)->impl_->cloud.setPairingToken(token);
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
        // Облачный MQTT-путь — гейтится ignoreExternalCmd.
        static_cast<Link*>(ctx)->dispatchCommand(command, data, /*fromLocal=*/false);
    }, this);
    impl_->cloud.setClaimCompleteCallback([](const char* deviceId, void* ctx) {
        auto* self = static_cast<Link::Impl*>(ctx);
        if (self->onClaimComplete) self->onClaimComplete(deviceId);
    }, impl_);
    impl_->cloud.setDiagnosticCallback([](const char* message, void* ctx) {
        auto* self = static_cast<Link::Impl*>(ctx);
        if (self->onDiagnostic) self->onDiagnostic(message);
    }, impl_);

    // Шина ошибок ESP-стороны: без обработчика всё, что в неё кладут, тихо
    // пропадает. Раньше проводку делал только iHeater-link, а touch и link не
    // делали вовсе. Ставим обработчик по умолчанию здесь; продукт может
    // переопределить его своим error_set_handler() уже после begin().
    error_set_handler([](const ErrorEvent* ev) {
        Link* self = s_selfForErrors;
        if (!self || !ev) return;
        EventKind kind;
        switch (ev->severity) {
            case ERRSEV_INFO:     kind = EventKind::Info;     break;
            case ERRSEV_WARNING:  kind = EventKind::Warning;  break;
            case ERRSEV_CRITICAL: kind = EventKind::Critical; break;
            default:              kind = EventKind::Error;    break;
        }
        char eventKey[48];
        snprintf(eventKey, sizeof(eventKey), "%s_%s",
                 errsrc_name(ev->source), errcode_name(ev->code));
        // CORE и LINK — про устройство целиком, а не про юнит: их ctrl_id
        // всегда 0 и превратился бы в «U1», вешая ошибку связи на первый юнит.
        const bool deviceWide = (ev->source == ERRSRC_CORE || ev->source == ERRSRC_LINK);
        self->raiseEvent(kind, eventKey, ev->msg, deviceWide ? 0xFF : ev->ctrl_id);
    });
    s_selfForErrors = this;

    // Причина прошлой перезагрузки — до старта runtime, чтобы счётчик уже был
    // готов к первому же выходу в онлайн.
    noteResetReason();

    // Bring runtime online.
    impl_->runtime.begin();

    return true;
}

// ── Ненормальные перезагрузки ────────────────────────────────────────────
//
// Причина сброса живёт ровно до следующей загрузки, а логи по USB видит только
// тот, кто сидит рядом с платой (паника через CDC теряется целиком: порт
// переподключается раньше, чем текст уходит). Поэтому счётчик кладётся в NVS и
// снимается ТОЛЬКО после успешной публикации: первая попытка приходится на
// самый занятый момент — сразу после подключения устройство шлёт info, card,
// телеметрию и меню, и событие может не влезть в очередь.
//
// Наблюдалось на живой плате ESP32-2424S012: касание зарядной микросхемы роняет
// её по BROWNOUT, но в портал доезжало одно сообщение из десяти.

const char* Link::resetReasonName(int reason) {
    switch ((esp_reset_reason_t)reason) {
        case ESP_RST_PANIC:    return "PANIC";     // исключение в коде
        case ESP_RST_INT_WDT:  return "INT_WDT";   // зависание в прерывании
        case ESP_RST_TASK_WDT: return "TASK_WDT";  // задача не отдала процессор
        case ESP_RST_WDT:      return "WDT";
        case ESP_RST_BROWNOUT: return "BROWNOUT";  // просадка питания
        default:               return nullptr;     // штатная загрузка
    }
}

// Полное имя причины — только для лога. Штатные причины сюда попадают тоже:
// при разборе сбоя важно отличать «выдернули питание» от «сами перезагрузились
// после OTA», а resetReasonName() их намеренно не различает.
static const char* resetReasonFullName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:  return "POWERON";
        case ESP_RST_EXT:      return "EXT";
        case ESP_RST_SW:       return "SW";
        case ESP_RST_DEEPSLEEP:return "DEEPSLEEP";
        case ESP_RST_SDIO:     return "SDIO";
        case ESP_RST_PANIC:    return "PANIC";
        case ESP_RST_INT_WDT:  return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT:      return "WDT";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        default:               return "UNKNOWN";
    }
}

// Сторож памяти. Смотрим не общий свободный объём, а самый крупный непрерывный
// блок: TLS-handshake, SUBSCRIBE и сборка JSON падают именно на нём, при вполне
// приличном суммарном free. Порог 8 КБ выбран по замерам живой платы: ниже него
// mbedtls уже не поднимался.
//
// Сообщаем один раз на вход в опасную зону, а не каждую проверку: иначе портал
// зальёт одинаковыми событиями. Возврат к норме (двойной порог — гистерезис,
// чтобы не дребезжало на границе) снимает защёлку.
void Link::checkLowMemory() {
    if (HAL_MILLIS() - lowMemLastCheckMs_ < LOW_MEM_CHECK_MS) return;
    lowMemLastCheckMs_ = HAL_MILLIS();

    const uint32_t largest =
        (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    if (!lowMemReported_ && largest < LOW_MEM_THRESHOLD) {
        lowMemReported_ = true;
        POST_ERROR(ERRSEV_WARNING, 0, ERRSRC_CORE, ERRC_LOW_MEMORY,
                   "largest free block low", (int32_t)largest);
        HAL_LOG_WARN("MEM", "low memory: largest free block %u", (unsigned)largest);
    } else if (lowMemReported_ && largest > LOW_MEM_THRESHOLD * 2) {
        lowMemReported_ = false;
        HAL_LOG_INFO("MEM", "memory recovered: largest free block %u", (unsigned)largest);
    }
}

void Link::noteResetReason() {
    const esp_reset_reason_t r = esp_reset_reason();
    const char* name = resetReasonName((int)r);

    // Причина каждого старта — сразу в Serial, а не через HAL: логи HAL
    // включаются только после подъёма Wi-Fi, то есть спустя секунды, а паника
    // через USB CDC теряется целиком. Это единственный надёжный след.
    Serial.printf("[BOOT] reset reason: %s (%d)\n", resetReasonFullName(r), (int)r);
    Serial.flush();

    Preferences prefs;
    if (!prefs.begin("sys", false)) return;
    abnPending_ = prefs.getUChar("abnCount", 0);
    if (name) {
        if (abnPending_ < 255) abnPending_++;
        prefs.putUChar("abnCount", abnPending_);
        prefs.putUChar("abnLast", (uint8_t)r);
        abnLastReason_ = (int)r;
        HAL_LOG_ERROR("BOOT", "abnormal reset: %s (не доложено: %u)",
                      name, (unsigned)abnPending_);
    } else if (abnPending_ > 0) {
        // Этот сброс штатный, но с прошлого раза остались недоложенные.
        abnLastReason_ = prefs.getUChar("abnLast", 0);
    }
    prefs.end();
}

void Link::reportAbnormalResets() {
    if (abnPending_ == 0 || !isOnline()) return;

    const char* name = resetReasonName(abnLastReason_);
    StaticJsonDocument<192> ev;
    ev["severity"] = "ERROR";
    ev["source"]   = "CORE";
    ev["event"]    = "CORE_ABNORMAL_RESET";
    ev["message"]  = name ? name : "UNKNOWN";
    ev["count"]    = abnPending_;      // сколько накопилось с прошлого доклада
    ev["unitId"]   = "DEVICE";
    if (!impl_->pub.publishEvent(ev)) return;   // повторим на следующем тике

    HAL_LOG_ERROR("BOOT", "reported abnormal resets: %s x%u",
                  name ? name : "UNKNOWN", (unsigned)abnPending_);
    abnPending_ = 0;
    Preferences prefs;
    if (prefs.begin("sys", false)) {
        prefs.remove("abnCount");
        prefs.remove("abnLast");
        prefs.end();
    }
}

namespace {

// «Активный» режим юнита = всё, что не Idle/Fault/Unknown. Единый критерий
// для session-tracking (publishStatusNow) и выбора периода телеметрии (loop).
inline bool isActiveUnitMode(UnitMode m) {
    return m != UnitMode::Idle && m != UnitMode::Fault && m != UnitMode::Unknown;
}

}  // namespace

void Link::loop() {
    // Throttled NVS-persist для накопительного workTimeCounter — каждые 5 мин.
    // No-op в большинстве итераций (внутренний interval check).
    idryer::WorkTimeTracker::instance().loop();

#ifndef IDRYER_DEV_REPL
    // До WiFi: только Improv. runtime.loop() → cloud.loop() →
    // WiFi.scanNetworks (~5с) переполняет USB CDC FIFO и ломает Improv-RPC.
    if (!impl_->logsEnabled) {
        impl_->improv.handleSerial();
        // Провижининг по воздуху крутится здесь же, до раннего выхода: пока
        // Wi-Fi нет, cloud.loop() не работает и радио свободно.
        idryer::EspTouchProvisioner::instance().loop();
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
        static char   s_serial_buf[128]; // binding-v3: вмещает PAIR_TOKEN:<токен>
        static uint8_t s_serial_len = 0;
        while (Serial.available() > 0) {
            char c = (char)Serial.read();
            if (c == '\r' || c == '\n') {
                if (s_serial_len > 0) {
                    s_serial_buf[s_serial_len] = '\0';
                    s_serial_len = 0;
                    const char* cmd = s_serial_buf;
                    if (strncmp(cmd, "PAIR_TOKEN:", 11) == 0) {
                        const char* tok = cmd + 11;
                        // Секрет уже есть → окно привязки закрыто. Тот же ответ,
                        // что даёт локальный WS (pair_fail already_bound): иначе
                        // провод отвечал «OK», токен молча терялся, и флешер
                        // показывал успех там, где ничего не произошло.
                        if (impl_->cloud.getIdentity().token[0] != '\0') {
                            Serial.println("PAIR_TOKEN:ERROR:ALREADY_BOUND");
                        } else if (tok[0] != '\0') {
                            impl_->cloud.setPairingToken(tok);
                            Serial.println("PAIR_TOKEN:OK");
                        } else {
                            Serial.println("PAIR_TOKEN:ERROR");
                        }
                        Serial.flush();
                    }
                    // binding-v3: factory-reset идентичности (стереть секрет/
                    // deviceId, сохранив WiFi) — вернуть устройство в SETUP.
                    // Для переустановки/повторной привязки и E2E-тестов.
                    else if (strcmp(cmd, "STATUS") == 0) {
                        // Факты для флешера вместо догадок «прошито, в сети»:
                        // состояние привязки, Wi-Fi, адрес, кто мы. Отвечаем
                        // только после подъёма Wi-Fi — до него Serial держит
                        // Improv; молчание = сети ещё нет.
                        const bool bound = impl_->cloud.getIdentity().token[0] != '\0';
                        const bool wifi  = WiFi.status() == WL_CONNECTED;
                        const char* mcu  = mcuSerial();
                        Serial.printf("STATUS:state=%s wifi=%d ip=%s cloud=%s serial=%s mcu=%s fw=%s\n",
                                      bound ? "bound" : "setup",
                                      wifi ? 1 : 0,
                                      wifi ? WiFi.localIP().toString().c_str() : "-",
                                      impl_->cloud.isOnline() ? "online" : "offline",
                                      serial(),
                                      (mcu && mcu[0]) ? mcu : "-",
                                      impl_->cfg.firmwareVersion ? impl_->cfg.firmwareVersion : "-");
                        Serial.flush();
                    }
                    else if (strcmp(cmd, "WIPE_IDENTITY") == 0) {
                        // Через Link::handleRevoke, а не cloud напрямую: иначе
                        // локальный транспорт держит старый токен — окно pair по
                        // WS закрыто, mDNS объявляет bound (инвариант §4b И-1).
                        handleRevoke();
                        Serial.println("WIPE_IDENTITY:OK");
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
#else
    // В dev-режиме Improv-ветки выше нет — провижининг крутим отсюда.
    idryer::EspTouchProvisioner::instance().loop();
#endif

    impl_->runtime.loop();
    // Шина ошибок ESP-стороны: без этого вызова события в ней просто копятся.
    error_process_all();
    reportAbnormalResets();
    checkLowMemory();
    if (impl_->localStarted) {
        impl_->local.loop();
        // local.begin() отработал ещё в SETUP с пустым токеном, а узнать новый
        // он мог только по провалу авторизации WS. Из-за этого после активации
        // mDNS продолжал объявлять state=setup, а окно pair по WS оставалось
        // открытым до перезагрузки. Секрет появился — отдаём сразу.
        const char* tok = impl_->cloud.getIdentity().token;
        if (tok[0] != '\0' && !impl_->local.hasToken()) impl_->local.updateToken(tok);
    }
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

    // Entity manifest (retained): публикуем после MQTT-коннекта и
    // перепубликуем, если продукт изменил декларацию (card().dirty()).
    if (impl_->pub.isMqttConnected()) {
        if (!impl_->cardPublished || impl_->card.dirty()) publishCardNow();
    } else {
        impl_->cardPublished = false;
    }

    if (anyTransport) {
        // Idle-периоды: когда ни один юнит не активен, публикуем реже
        // (*PeriodIdleMs; 0 = не различать active/idle).
        bool anyActive = false;
        for (uint8_t i = 0; i < impl_->cfg.unitsCount && i < MAX_UNITS; ++i) {
            if (isActiveUnitMode(status.mode[i])) { anyActive = true; break; }
        }

        uint32_t telemetryPeriod = impl_->cfg.telemetryPeriodMs;
        if (!anyActive && impl_->cfg.telemetryPeriodIdleMs > 0) {
            telemetryPeriod = impl_->cfg.telemetryPeriodIdleMs;
        }
        // Событийно по булевым полям (fan/servo): смена состояния публикует
        // телеметрию сразу; дебаунс 2с защищает от дребезга. Периодика — сверка.
        bool binaryChanged = false;
        for (uint8_t i = 0; i < impl_->cfg.unitsCount && i < MAX_UNITS; ++i) {
            if ((impl_->cfg.hasFan   && telemetry.fanOn[i]     != impl_->lastPubFanOn[i]) ||
                (impl_->cfg.hasServo && telemetry.servoOpen[i] != impl_->lastPubServoOpen[i])) {
                binaryChanged = true;
                break;
            }
        }
        if (telemetryPeriod > 0 &&
            ((binaryChanged && now - impl_->lastTelemetryMs >= 2000) ||
             now - impl_->lastTelemetryMs >= telemetryPeriod)) {
            impl_->lastTelemetryMs = now;
            publishTelemetryNow();
        }

        // Status: событийно (изменение значимых полей — mode/target/duration)
        // + периодика-сверка. Volatile-поля (elapsed/rssi/uptime) изменением
        // НЕ считаются — иначе каждый снапшот «новый» и событийность теряет
        // смысл. Кастомные поля продуктов (onStatusPublish hook) SDK не видит —
        // их события продукт публикует сам через publishStatusNow().
        if (impl_->cfg.statusPeriodMs > 0) {
            bool statusChanged = false;
            for (uint8_t i = 0; i < impl_->cfg.unitsCount && i < MAX_UNITS; ++i) {
                if (status.mode[i]        != impl_->lastPubMode[i] ||
                    status.targetTempC[i] != impl_->lastPubTargetC[i] ||
                    status.durationS[i]   != impl_->lastPubDurationS[i]) {
                    statusChanged = true;
                    break;
                }
            }

            uint32_t statusPeriod = impl_->cfg.statusPeriodMs;
            if (!anyActive && impl_->cfg.statusPeriodIdleMs > 0) {
                statusPeriod = impl_->cfg.statusPeriodIdleMs;
            }

            if (statusChanged || now - impl_->lastStatusMs >= statusPeriod) {
                impl_->lastStatusMs = now;
                publishStatusNow();
            }
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

idryer::CardBuilder& Link::card() {
    return impl_->card;
}

void Link::publishCardNow() {
    // 2048: до 16 объявленных сущностей + авто-сенсоры + layout с запасом.
    DynamicJsonDocument doc(2048);
    impl_->card.buildJson(doc, impl_->cfg);
    if (impl_->pub.publishCard(doc)) {
        impl_->cardPublished = true;
        impl_->card.clearDirty();
    }
}

void Link::publishTelemetryNow() {
    const Config& cfg = impl_->cfg;
    // 640: 4 юнита × ~7 полей (включая servoOpen) с запасом; 512 было впритык.
    StaticJsonDocument<640> doc;

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
        if (cfg.hasServo)  u["servoOpen"]   = telemetry.servoOpen[i];

        // Снапшот булевых полей — для событийной публикации в loop().
        impl_->lastPubFanOn[i]     = telemetry.fanOn[i];
        impl_->lastPubServoOpen[i] = telemetry.servoOpen[i];
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

        // Снапшот для событийной публикации (см. loop).
        impl_->lastPubMode[i]      = status.mode[i];
        impl_->lastPubTargetC[i]   = status.targetTempC[i];
        impl_->lastPubDurationS[i] = status.durationS[i];

        // sessionNum: backend requires > 0 for DRYING/STORAGE/PROFILE.
        // Источник истины — MCU: у него счётчик пер-юнитовый и лежит в EEPROM,
        // поэтому номер переживает перезагрузку Link (OTA, eraseClaimAndRestart)
        // и не рвёт живую сессию в БД. Локальный счётчик остаётся фолбэком для
        // продуктов, которые status.sessionNum не заполняют (шлют 0).
        // Phase 5: добавили Heating, LightAnimation, Profile. Generic-проверка
        // (isActiveUnitMode) вместо whitelist, чтобы новые mode'ы автоматически
        // попадали в session-tracking без правки SDK.
        const bool wasActive = isActiveUnitMode(impl_->lastModeForSn[i]);
        const bool isActive  = isActiveUnitMode(status.mode[i]);
        if (isActive && !wasActive) impl_->sessionNum[i]++;
        impl_->lastModeForSn[i] = status.mode[i];

        const uint32_t mcuSessionNum = status.sessionNum[i];
        u["sessionNum"] = isActive ? (mcuSessionNum ? mcuSessionNum : impl_->sessionNum[i]) : 0;

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
    // ignoreExternalCmd больше НЕ шлём в status: значение — это пункт меню с
    // canonical role system.ignore_external_cmd, и оно доставляется через
    // config (на connect) и config/delta (на изменение). Дублировать его в
    // каждом status (~каждые 10с) незачем. Портал читает флаг из config-пути;
    // legacy-приём из status оставлен на портале только для старых прошивок.
    // Внутренний guard команд по-прежнему использует impl_->ignoreExternalCmd.

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

void Link::handleRevoke() {
    impl_->cloud.handleRevoke();
    // binding-v3: cloud стёр секрет и ушёл в SETUP, но у local_access остаётся
    // старый deviceToken_ от local.begin() — без этого WS-окно пейринга и после
    // REVOKE/WIPE отвечало бы pair_fail already_bound до перезагрузки. Стираем
    // токен у локального транспорта, чтобы окно привязки открылось сразу
    // (инвариант §5: отвязанное устройство = SETUP, тот же путь входа, без ребута).
    if (impl_->localStarted) impl_->local.clearToken();
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

void Link::dispatchCommand(const char* command, JsonObjectConst data, bool fromLocal) {
    if (!command || !command[0]) return;

    // Origin виден product-хендлеру через currentCommandFromLocal() — например
    // idryer-link ставит UART_FLAG_LOCAL при форварде action-команды на RP2040.
    impl_->currentCmdFromLocal = fromLocal;

    // ─── Gate: ignoreExternalCmd ─────────────────────────────────────────
    // Гейтит только ОБЛАЧНЫЙ путь (MQTT). Локальные команды (fromLocal=true —
    // WS в LAN, под токеном) проходят ВСЕГДА: смысл флага — «портал/облако мной
    // не управляет, а локальная мобилка в сети — да». Источник задаёт вызывающий
    // (проводка sink'ов), из payload не берётся — облако не подделает fromLocal.
    //
    // Для облака: защищает от внешних команд-ДЕЙСТВИЙ (запуск нагрева, включение
    // ленты, запись RFID и т.п.). Не блокирует read/config: пользователь должен
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
    if (impl_->ignoreExternalCmd && !fromLocal) {
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
    } else if (strcmp(command, "get_info") == 0) {
        // Пуш info (несёт mcuSerial). Используется локальным WS при auth_ok,
        // чтобы клиент получил идентичность RP2040 сразу, не дожидаясь события.
        publishInfoNow();
        builtinHandled = true;
    }

    // ─── Card-контролы: invoke card.{id} → колбэк CardBuilder ─────────────
    // Перехватываем ДО продуктового registry: продуктовый onCommand("invoke")
    // не обязан знать про card.* actions.
    if (strcmp(command, "invoke") == 0) {
        const char* action = data["action"].as<const char*>();
        if (action && strncmp(action, "card.", 5) == 0) {
            if (!impl_->card.handleInvokeAction(action, data["args"])) {
                HAL_LOG_WARN("LINK", "unknown card action: %s", action);
            }
            return;
        }
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


void Link::onClaimComplete(ClaimCompleteCallback cb) {
    impl_->onClaimComplete = std::move(cb);
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

bool Link::isBound() const {
    return impl_->cloud.getIdentity().token[0] != '\0';
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
    // Отдаём креды и работающему менеджеру, а не только в NVS: он стартовал в
    // begin() с тем, что было в памяти на тот момент, и без этого продолжит
    // переподключаться со старыми (или пустыми) значениями до перезагрузки.
    // Тот же порядок, что в пути Improv выше.
    impl_->wifi.begin(ssid, password);
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

bool Link::currentCommandFromLocal() const {
    return impl_->currentCmdFromLocal;
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

void Link::setMcuHardwareVersion(const char* hwVersion) {
    impl_->cloud.setMcuHardwareVersion(hwVersion);
}

void Link::setMcuWorkTimeCounter(uint32_t seconds) {
    impl_->cloud.setMcuWorkTimeCounter(seconds);
}

const char* Link::mcuSerial() const {
    return impl_->cloud.getMcuSerial();
}

const char* Link::mcuHardwareVersion() const {
    return impl_->cloud.getMcuHardwareVersion();
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
