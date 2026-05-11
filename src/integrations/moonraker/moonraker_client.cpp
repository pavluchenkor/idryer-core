/**
 * @file moonraker_client.cpp
 * @brief Реализация клиента Moonraker (Klipper) через WebSocket JSON-RPC.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "moonraker_client.h"
#include "../../hal/hal_types.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

namespace idryer {
namespace cloud {

const char* moonrakerConnectionStateToString(MoonrakerConnectionState value)
{
    switch (value)
    {
    case MoonrakerConnectionState::Disabled:   return "disabled";
    case MoonrakerConnectionState::Idle:       return "idle";
    case MoonrakerConnectionState::Connecting: return "connecting";
    case MoonrakerConnectionState::Connected:  return "connected";
    case MoonrakerConnectionState::Error:      return "error";
    }
    return "disabled";
}

// =============================================================================
// Constructor / destructor
// =============================================================================

MoonrakerClient::MoonrakerClient()
{
    ws_.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->onWebSocketEvent(type, payload, length);
    });
}

MoonrakerClient::~MoonrakerClient()
{
    shutdown();
}

// =============================================================================
// configure / shutdown
// =============================================================================

void MoonrakerClient::configure(const MoonrakerConfig& cfg)
{
    HAL_LOG_INFO("MOON", "configure() in: enabled=%d host='%s' port=%u ssl=%d apiKey=%s",
                 cfg.enabled ? 1 : 0,
                 cfg.host[0] ? cfg.host : "<empty>",
                 cfg.port,
                 cfg.ssl ? 1 : 0,
                 cfg.apiKey[0] ? "<set>" : "<empty>");

    bool identical =
        configValid_
        && cfg.enabled == cfg_.enabled
        && strcmp(cfg.host, cfg_.host) == 0
        && cfg.port == cfg_.port
        && cfg.ssl  == cfg_.ssl
        && strcmp(cfg.apiKey, cfg_.apiKey) == 0;

    if (identical) {
        HAL_LOG_INFO("MOON", "configure: identical config, skip");
        return;
    }

    ws_.disconnect();

    cfg_ = cfg;
    configValid_ = cfg_.configured();
    status_ = MoonrakerStatus{};

    HAL_LOG_INFO("MOON", "configure: configValid=%d (cfg.configured() check)",
                 configValid_ ? 1 : 0);

    if (!configValid_) {
        HAL_LOG_WARN("MOON", "configure: NOT starting WS (host empty)");
        setState(MoonrakerConnectionState::Idle);
        setError("configuration incomplete (need host)");
        return;
    }

    char path[96];
    if (cfg_.apiKey[0] != '\0') {
        snprintf(path, sizeof(path), "%s?token=%s", kWsPath, cfg_.apiKey);
    } else {
        snprintf(path, sizeof(path), "%s", kWsPath);
    }

    HAL_LOG_INFO("MOON", "ws.begin() %s://%s:%u%s",
                 cfg_.ssl ? "wss" : "ws", cfg_.host, cfg_.port, path);
    if (cfg_.ssl) {
        ws_.beginSSL(cfg_.host, cfg_.port, path);
    } else {
        ws_.begin(cfg_.host, cfg_.port, path);
    }

    // Убираем дефолтный `Origin: file://` — Moonraker проверяет Origin
    // против `cors_domains`, и `file://` там обычно нет → 403 Forbidden.
    ws_.setExtraHeaders("");
    ws_.setReconnectInterval(reconnectBackoffMs_);

    reconnectBackoffMs_ = kReconnectMinMs;
    nextConnectAttemptMs_ = millis();
    setState(MoonrakerConnectionState::Connecting);
    setError("");

    HAL_LOG_INFO("MOON", "configure: %s://%s:%u%s",
                 cfg_.ssl ? "wss" : "ws",
                 cfg_.host, cfg_.port, kWsPath);
}

void MoonrakerClient::shutdown()
{
    ws_.disconnect();
    configValid_ = false;
    // Fail-safe: при отключении интеграции обнуляем target и сообщаем продукту,
    // иначе auto_heat останется на последнем target от Moonraker даже после
    // переключения на Bambu/HA или ручного отключения.
    bool wasActive = status_.virtualChamberAvailable
                  || status_.chamberTarget != 0.0f;
    status_ = MoonrakerStatus{};
    if (wasActive) {
        if (vcCallback_) {
            VirtualChamberData data{};
            data.available   = false;
            data.hasSensor   = false;
            data.target      = 0.0f;
            data.temperature = 0.0f;
            vcCallback_(data);
        }
        if (chamberCallback_) chamberCallback_(0.0f, false);
        if (statusCallback_)  statusCallback_(status_);
    }
    setState(MoonrakerConnectionState::Disabled);
}

// =============================================================================
// loop
// =============================================================================

void MoonrakerClient::loop()
{
    if (state_ == MoonrakerConnectionState::Disabled
        || state_ == MoonrakerConnectionState::Idle)
    {
        return;
    }
    ws_.loop();
}

// =============================================================================
// WebSocket events
// =============================================================================

void MoonrakerClient::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
    switch (type)
    {
    case WStype_CONNECTED:
        HAL_LOG_INFO("MOON", "ws connected: %s", payload ? (const char*)payload : "");
        setState(MoonrakerConnectionState::Connected);
        setError("");
        reconnectBackoffMs_ = kReconnectMinMs;
        ws_.setReconnectInterval(reconnectBackoffMs_);
        sendSubscribe();
        break;

    case WStype_DISCONNECTED: {
        HAL_LOG_WARN("MOON", "ws disconnected (backoff=%ums) — chamber target → 0 (safety)",
                     reconnectBackoffMs_);
        // Fail-safe: при потере соединения обнуляем chamber target и сообщаем
        // продукту, чтобы auto_heat выключил нагрев. Иначе нагрев продолжится
        // на последнем известном target пока коннект не восстановится.
        bool wasActive = status_.virtualChamberAvailable
                      || status_.chamberTarget != 0.0f
                      || status_.chamberTemperature != 0.0f;
        status_ = MoonrakerStatus{};
        if (wasActive) {
            if (vcCallback_) {
                VirtualChamberData data{};
                data.available   = false;
                data.hasSensor   = false;
                data.target      = 0.0f;
                data.temperature = 0.0f;
                vcCallback_(data);
            }
            if (chamberCallback_) chamberCallback_(0.0f, false);
            if (statusCallback_)  statusCallback_(status_);
        }
        setState(MoonrakerConnectionState::Connecting);
        reconnectBackoffMs_ *= 2;
        if (reconnectBackoffMs_ > kReconnectMaxMs) reconnectBackoffMs_ = kReconnectMaxMs;
        ws_.setReconnectInterval(reconnectBackoffMs_);
        break;
    }

    case WStype_TEXT:
        if (payload && length > 0) {
            if (logPayloads_) {
                unsigned int prevLen = length < 400u ? length : 400u;
                HAL_LOG_INFO("MOON", "← RAW[%u]: %.*s",
                              (unsigned)length, (int)prevLen, (const char*)payload);
            }
            handleJsonRpcMessage((const char*)payload, length);
        }
        break;

    case WStype_ERROR:
        setError("ws error");
        HAL_LOG_WARN("MOON", "ws error: %.*s",
                     (int)length, payload ? (const char*)payload : "");
        break;

    case WStype_PING:
        HAL_LOG_DEBUG("MOON", "ws ping");
        break;
    case WStype_PONG:
        HAL_LOG_DEBUG("MOON", "ws pong");
        break;
    case WStype_BIN:
        HAL_LOG_DEBUG("MOON", "ws ← bin (%u bytes)", (unsigned)length);
        break;

    default:
        HAL_LOG_DEBUG("MOON", "ws event type=%d len=%u", (int)type, (unsigned)length);
        break;
    }
}

// =============================================================================
// JSON-RPC
// =============================================================================

void MoonrakerClient::sendSubscribe()
{
    StaticJsonDocument<512> doc;
    doc["jsonrpc"] = "2.0";
    doc["method"]  = "printer.objects.subscribe";

    JsonObject params  = doc.createNestedObject("params");
    JsonObject objects = params.createNestedObject("objects");

    JsonArray vcFields = objects.createNestedArray("gcode_macro VIRTUAL_CHAMBER");
    vcFields.add("target");
    vcFields.add("temperature");
    vcFields.add("has_sensor");

    objects["print_stats"]      = nullptr;
    objects["virtual_sdcard"]   = nullptr;
    objects["display_status"]   = nullptr;

    JsonArray extruderFields = objects.createNestedArray("extruder");
    extruderFields.add("temperature");
    extruderFields.add("target");

    JsonArray bedFields = objects.createNestedArray("heater_bed");
    bedFields.add("temperature");
    bedFields.add("target");

    doc["id"] = nextRpcId_++;

    String out;
    serializeJson(doc, out);
    ws_.sendTXT(out);
    HAL_LOG_INFO("MOON", "subscribe sent (%u bytes)", (unsigned)out.length());
}

void MoonrakerClient::handleJsonRpcMessage(const char* text, size_t length)
{
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, text, length);
    if (err) {
        HAL_LOG_WARN("MOON", "json parse err: %s", err.c_str());
        return;
    }

    const char* method = doc["method"] | (const char*)nullptr;
    if (method && strcmp(method, "notify_status_update") == 0) {
        JsonArrayConst params = doc["params"].as<JsonArrayConst>();
        if (!params.isNull() && params.size() >= 1) {
            JsonObjectConst statusObj = params[0].as<JsonObjectConst>();
            if (!statusObj.isNull()) {
                applyStatusUpdate(statusObj);
            }
        }
        return;
    }

    if (doc.containsKey("result")) {
        JsonObjectConst result = doc["result"].as<JsonObjectConst>();
        if (!result.isNull()) {
            JsonObjectConst statusObj = result["status"].as<JsonObjectConst>();
            if (!statusObj.isNull()) {
                applyStatusUpdate(statusObj);
            }
        }
        return;
    }

    if (doc.containsKey("error")) {
        const char* msg = doc["error"]["message"] | "rpc error";
        HAL_LOG_WARN("MOON", "rpc error: %s", msg);
        setError(msg);
    }
}

void MoonrakerClient::applyStatusUpdate(const JsonObjectConst& statusObj)
{
    bool chamberChanged = false;
    bool vcStructuralChange = false;
    bool otherChanged   = false;

    JsonObjectConst vc = statusObj["gcode_macro VIRTUAL_CHAMBER"].as<JsonObjectConst>();
    if (!vc.isNull()) {
        if (!status_.virtualChamberAvailable) {
            status_.virtualChamberAvailable = true;
            vcStructuralChange = true;
            chamberChanged = true;
        }

        if (vc.containsKey("target")) {
            float target = vc["target"].as<float>();
            if (target != status_.chamberTarget) {
                status_.chamberTarget = target;
                chamberChanged = true;
                vcStructuralChange = true;
            }
        }

        if (vc.containsKey("temperature")) {
            float temp = vc["temperature"].as<float>();
            if (temp != status_.chamberTemperature) {
                status_.chamberTemperature = temp;
                chamberChanged = true;
            }
        }

        if (vc.containsKey("has_sensor")) {
            bool hs = vc["has_sensor"].as<int>() != 0;
            if (hs != status_.chamberHasSensor) {
                status_.chamberHasSensor = hs;
                chamberChanged = true;
                vcStructuralChange = true;
            }
        }
    }

    JsonObjectConst ps = statusObj["print_stats"].as<JsonObjectConst>();
    if (!ps.isNull()) {
        const char* s = ps["state"] | (const char*)nullptr;
        if (s) {
            if (strncmp(status_.printerState, s, sizeof(status_.printerState) - 1) != 0) {
                strncpy(status_.printerState, s, sizeof(status_.printerState) - 1);
                status_.printerState[sizeof(status_.printerState) - 1] = '\0';
                otherChanged = true;
            }
        }
        const char* fn = ps["filename"] | (const char*)nullptr;
        if (fn) {
            if (strncmp(status_.filename, fn, sizeof(status_.filename) - 1) != 0) {
                strncpy(status_.filename, fn, sizeof(status_.filename) - 1);
                status_.filename[sizeof(status_.filename) - 1] = '\0';
                otherChanged = true;
            }
        }
        if (ps.containsKey("print_duration")) {
            uint32_t d = (uint32_t)(ps["print_duration"].as<float>());
            if (d != status_.printDurationSeconds) {
                status_.printDurationSeconds = d;
                otherChanged = true;
            }
        }
    }

    JsonObjectConst ds = statusObj["display_status"].as<JsonObjectConst>();
    if (!ds.isNull() && ds.containsKey("progress")) {
        float p = ds["progress"].as<float>() * 100.0f;
        if (p != status_.progress) {
            status_.progress = p;
            otherChanged = true;
        }
    }

    JsonObjectConst ext = statusObj["extruder"].as<JsonObjectConst>();
    if (!ext.isNull()) {
        if (ext.containsKey("temperature")) {
            float t = ext["temperature"].as<float>();
            if (t != status_.nozzleTemp) { status_.nozzleTemp = t; otherChanged = true; }
        }
        if (ext.containsKey("target")) {
            float t = ext["target"].as<float>();
            if (t != status_.nozzleTarget) { status_.nozzleTarget = t; otherChanged = true; }
        }
    }
    JsonObjectConst bed = statusObj["heater_bed"].as<JsonObjectConst>();
    if (!bed.isNull()) {
        if (bed.containsKey("temperature")) {
            float t = bed["temperature"].as<float>();
            if (t != status_.bedTemp) { status_.bedTemp = t; otherChanged = true; }
        }
        if (bed.containsKey("target")) {
            float t = bed["target"].as<float>();
            if (t != status_.bedTarget) { status_.bedTarget = t; otherChanged = true; }
        }
    }

    if (chamberChanged || otherChanged) {
        HAL_LOG_INFO("MOON",
                     "← state=%s progress=%.0f%% nozzle=%.0f/%.0f bed=%.0f/%.0f vc: target=%.1f temp=%.1f hasSensor=%d available=%d",
                     status_.printerState[0] ? status_.printerState : "-",
                     (double)status_.progress,
                     (double)status_.nozzleTemp,   (double)status_.nozzleTarget,
                     (double)status_.bedTemp,       (double)status_.bedTarget,
                     (double)status_.chamberTarget,
                     (double)status_.chamberTemperature,
                     status_.chamberHasSensor ? 1 : 0,
                     status_.virtualChamberAvailable ? 1 : 0);
    }

    if (vcStructuralChange && chamberCallback_) {
        chamberCallback_(status_.chamberTarget, status_.virtualChamberAvailable);
    }

    if (chamberChanged && vcCallback_) {
        VirtualChamberData data;
        data.available   = status_.virtualChamberAvailable;
        data.hasSensor   = status_.chamberHasSensor;
        data.target      = status_.chamberTarget;
        data.temperature = status_.chamberTemperature;
        vcCallback_(data);
    }

    if ((chamberChanged || otherChanged) && statusCallback_) {
        statusCallback_(status_);
    }
}

// =============================================================================
// Helpers
// =============================================================================

void MoonrakerClient::setState(MoonrakerConnectionState newState)
{
    if (state_ == newState) return;
    MoonrakerConnectionState old = state_;
    state_ = newState;
    HAL_LOG_INFO("MOON", "state: %s -> %s",
                 moonrakerConnectionStateToString(old),
                 moonrakerConnectionStateToString(newState));
    if (stateCallback_) stateCallback_(newState);
}

void MoonrakerClient::setError(const char* message)
{
    if (!message) message = "";
    strncpy(lastError_, message, sizeof(lastError_) - 1);
    lastError_[sizeof(lastError_) - 1] = '\0';
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
