#if defined(ESP32) || defined(ESP_PLATFORM)

#include "local_access.h"
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <hal/hal_types.h>

namespace idryer {

// ── WsImpl ───────────────────────────────────────────────────────────────────
// Subclass exposing protected sendFrame for fragmented delivery of large payloads
// without allocating a large intermediate buffer.

class LocalAccess::WsImpl : public WebSocketsServer {
public:
    explicit WsImpl(uint16_t port) : WebSocketsServer(port) {}

    void sendFragmented(uint8_t num,
                        const char* prefix, size_t prefixLen,
                        const char* json,   size_t jsonLen)
    {
        WSclient_t* client = &_clients[num];
        char suffix = '}';
        sendFrame(client, WSop_text,
            reinterpret_cast<uint8_t*>(const_cast<char*>(prefix)), prefixLen, false);
        sendFrame(client, WSop_continuation,
            reinterpret_cast<uint8_t*>(const_cast<char*>(json)), jsonLen, false);
        sendFrame(client, WSop_continuation,
            reinterpret_cast<uint8_t*>(&suffix), 1, true);
    }
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void LocalAccess::initMdns(const char* deviceName)
{
    if (!deviceName || deviceName[0] == '\0') {
        HAL_LOG_WARN("WS", "initMdns: deviceName empty");
        return;
    }
    strncpy(deviceName_, deviceName, sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';

    if (MDNS.begin(deviceName_)) {
        MDNS.addService("_idryer", "_tcp", 81);
        HAL_LOG_INFO("WS", "mDNS: %s.local → _idryer._tcp:81 (WS not yet started)", deviceName_);
    } else {
        HAL_LOG_WARN("WS", "mDNS: MDNS.begin failed for %s", deviceName_);
    }
}

void LocalAccess::begin(const char* deviceName, const char* deviceToken)
{
    if (enabled_) {
        HAL_LOG_WARN("WS", "begin: already enabled");
        return;
    }

    strncpy(deviceName_, deviceName ? deviceName : "", sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';
    strncpy(deviceToken_, deviceToken ? deviceToken : "", sizeof(deviceToken_) - 1);
    deviceToken_[sizeof(deviceToken_) - 1] = '\0';

    ws_ = new WsImpl(81);
    ws_->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        onWsEvent(num, static_cast<uint8_t>(type), payload, length);
    });
    ws_->begin();

    const bool mdnsOk = MDNS.begin(deviceName_);
    if (mdnsOk) {
        MDNS.addService("_idryer", "_tcp", 81);
    }

    enabled_ = true;
    HAL_LOG_INFO("WS", "Started: %s.local:81 mDNS=%s token=%s",
                 deviceName_,
                 mdnsOk ? "ok" : "fail",
                 deviceToken_[0] != '\0' ? "set" : "empty");
}

void LocalAccess::stop()
{
    if (!enabled_) return;
    if (ws_) { ws_->close(); delete ws_; ws_ = nullptr; }
    MDNS.end();
    enabled_         = false;
    connectedClient_ = -1;
    clientAuthorized_ = false;
    HAL_LOG_INFO("WS", "Stopped");
}

void LocalAccess::loop()
{
    if (!enabled_ || !ws_) return;
    ws_->loop();

    if (needsTokenRefresh_ && tokenRefreshCb_) {
        needsTokenRefresh_ = false;
        tokenRefreshCb_();
    }
}

bool LocalAccess::isListening() const
{
    return enabled_ && ws_ != nullptr;
}

void LocalAccess::updateToken(const char* newToken)
{
    if (!newToken || newToken[0] == '\0') return;
    strncpy(deviceToken_, newToken, sizeof(deviceToken_) - 1);
    deviceToken_[sizeof(deviceToken_) - 1] = '\0';
    HAL_LOG_INFO("WS", "Token updated");
}

// ── WS event dispatch ─────────────────────────────────────────────────────────

void LocalAccess::onWsEvent(uint8_t num, uint8_t type, uint8_t* payload, size_t length)
{
    WStype_t wsType = static_cast<WStype_t>(type);

    switch (wsType) {
    case WStype_CONNECTED:
        HAL_LOG_INFO("WS", ">>> CONNECTED #%d from %s", num, ws_->remoteIP(num).toString().c_str());
        if (connectedClient_ >= 0 && connectedClient_ != num) {
            HAL_LOG_WARN("WS", "Rejecting #%d — already have #%d", num, connectedClient_);
            ws_->disconnect(num);
            return;
        }
        connectedClient_  = num;
        clientAuthorized_ = false;
        break;

    case WStype_DISCONNECTED:
        HAL_LOG_INFO("WS", "<<< DISCONNECTED #%d (authorized=%d)", num, clientAuthorized_);
        if (connectedClient_ == num) {
            connectedClient_  = -1;
            clientAuthorized_ = false;
        }
        break;

    case WStype_TEXT:
        if (num == connectedClient_) {
            handleMessage(num, reinterpret_cast<const char*>(payload), length);
        }
        break;

    case WStype_ERROR:
        HAL_LOG_WARN("WS", "Error on client #%d", num);
        break;

    default:
        break;
    }
}

// ── Message handling ──────────────────────────────────────────────────────────
//
// WS message envelope:
//   incoming auth:    {"type":"auth","token":"<device_token>"}
//   incoming command: {"type":"command","command":"<name>","data":{...}}
//
// After unwrap, command and data are forwarded to CommandSink — the same
// handler the MQTT path uses. Transport envelope is never exposed to product.

void LocalAccess::handleMessage(uint8_t num, const char* json, size_t length)
{
    static StaticJsonDocument<1024> doc;
    doc.clear();
    if (deserializeJson(doc, json, length) != DeserializationError::Ok) {
        HAL_LOG_WARN("WS", "JSON parse error from #%d", num);
        return;
    }

    const char* type = doc["type"] | "";

    // ── Auth ──────────────────────────────────────────────────────────────────
    if (strcmp(type, "auth") == 0) {
        const char* token = doc["token"] | "";
        const bool  ok    = (deviceToken_[0] != '\0' && strcmp(token, deviceToken_) == 0);

        if (ok) {
            clientAuthorized_ = true;

            StaticJsonDocument<128> resp;
            resp["type"]       = "auth_ok";
            resp["deviceName"] = deviceName_;
            sendDoc(nullptr, resp);

            HAL_LOG_INFO("WS", "Auth OK — client #%d", num);

            // Immediately push current config so the app has a full picture.
            if (commandSink_) {
                StaticJsonDocument<1> empty;
                commandSink_("get_config", empty.as<JsonObjectConst>());
            }
        } else {
            StaticJsonDocument<64> resp;
            resp["type"]   = "auth_fail";
            resp["reason"] = "invalid_token";
            sendDoc(nullptr, resp);

            HAL_LOG_WARN("WS", "Auth failed — invalid token from #%d", num);
            needsTokenRefresh_ = true;
        }
        return;
    }

    // ── Require auth for all other messages ───────────────────────────────────
    if (!clientAuthorized_) {
        StaticJsonDocument<64> resp;
        resp["type"]   = "auth_fail";
        resp["reason"] = "not_authorized";
        sendDoc(nullptr, resp);
        return;
    }

    // ── Command ───────────────────────────────────────────────────────────────
    // Unwrap WS envelope and forward (command, data) to the shared CommandSink.
    // Transport envelope is stripped here — product sees only the command name
    // and its data, identical in shape to the MQTT command path.
    if (strcmp(type, "command") == 0) {
        const char*   command = doc["command"] | "";
        JsonObjectConst data  = doc["data"].as<JsonObjectConst>();

        if (command[0] != '\0' && commandSink_) {
            HAL_LOG_INFO("WS", "Command: %s", command);
            commandSink_(command, data);
        }
        return;
    }

    HAL_LOG_WARN("WS", "Unknown message type: %s", type);
}

// ── Outgoing data ─────────────────────────────────────────────────────────────

void LocalAccess::publish(const char* type, JsonDocument& doc)
{
    if (!isClientConnected()) return;
    sendDoc(type, doc);
}

void LocalAccess::publish(const char* type, const char* jsonRaw, size_t len)
{
    if (!isClientConnected() || !jsonRaw || len == 0) return;
    sendFragment(type, jsonRaw, len);
}

// Wraps doc as {"type":"...","data":{...}} and sends.
// When type == nullptr, sends doc as-is (used for auth responses).
void LocalAccess::sendDoc(const char* type, JsonDocument& doc)
{
    if (!ws_ || connectedClient_ < 0) return;

    if (type) {
        static StaticJsonDocument<2048> wrapper;
        static char buf[2048];
        wrapper.clear();
        wrapper["type"] = type;
        wrapper["data"] = doc.as<JsonObject>();
        const size_t len = serializeJson(wrapper, buf, sizeof(buf));
        HAL_LOG_INFO("WS", "TX type=%s %u bytes", type, static_cast<unsigned>(len));
        ws_->sendTXT(connectedClient_, buf, len);
    } else {
        static char buf[256];
        const size_t len = serializeJson(doc, buf, sizeof(buf));
        ws_->sendTXT(connectedClient_, buf, len);
    }
}

// Wraps pre-serialized JSON using fragmented send to avoid an extra buffer.
// Sends three frames: prefix + raw JSON + closing brace.
void LocalAccess::sendFragment(const char* type, const char* json, size_t len)
{
    if (!ws_ || connectedClient_ < 0) return;

    char prefix[48];
    const int prefixLen = snprintf(prefix, sizeof(prefix), "{\"type\":\"%s\",\"data\":", type);
    if (prefixLen <= 0 || static_cast<size_t>(prefixLen) >= sizeof(prefix)) {
        HAL_LOG_WARN("WS", "sendFragment: prefix overflow for type=%s", type);
        return;
    }

    HAL_LOG_INFO("WS", "TX(raw) type=%s %u bytes", type, static_cast<unsigned>(len));
    ws_->sendFragmented(connectedClient_,
                        prefix, static_cast<size_t>(prefixLen),
                        json, len);
}

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
