#pragma once

/**
 * @file local_access.h
 * @brief LAN/local transport layer: WebSocket server + mDNS discovery.
 *
 * Provides local access to the device API when MQTT cloud transport is
 * unavailable or the app operates in LAN-only mode.
 *
 * Architecture:
 *   MQTT transport  → topic unwrap → command sink (get_config / set / invoke / extras)
 *   Local WS        → envelope unwrap → same command sink
 *
 * Incoming WS messages are unwrapped from:
 *   {"type":"command", "command":"<name>", "data":{...}}
 * and forwarded to CommandSink as (name, data) — same as the MQTT command path.
 *
 * Outgoing data: product calls publish(type, doc) — this module wraps and sends
 *   {"type":"<type>", "data":{...}}
 *
 * mDNS: device advertises _idryer._tcp:81 using the serial number as hostname.
 * initMdns() can be called early (before WS starts) so apps can discover the
 * device immediately after WiFi connects.
 *
 * Auth: first message from client must be {"type":"auth","token":"<device_token>"}.
 * One client at a time. Invalid token triggers TokenRefreshCallback.
 *
 * Include separately — requires WebSocketsServer and ESPmDNS libraries:
 *   #include <local_access/local_access.h>
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include <functional>
#include <ArduinoJson.h>
#include "../core/config.h"

namespace idryer {

class LocalAccess {
public:
    /**
     * Incoming command after WS envelope unwrap.
     * @param command  Command name (e.g. "get_config", "invoke", "set").
     * @param data     JSON object from "data" field.
     *
     * Wired to the same handler the MQTT path uses. get_config is typically
     * intercepted before the ActionDispatcher — same rule applies here.
     */
    using CommandSink = std::function<void(const char* command, JsonObjectConst data)>;

    /** Called when client presents an invalid token. Product should reload token. */
    using TokenRefreshCallback = std::function<void()>;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * Start mDNS advertisement without starting the WS server.
     * Call as soon as the serial number is known so apps can discover the device
     * before the WS server is up.
     * @param deviceName  Device serial number (used as mDNS hostname).
     */
    void initMdns(const char* deviceName);

    /**
     * Start the WebSocket server (port 81) and mDNS.
     * @param deviceName   Device serial number.
     * @param deviceToken  Token for client auth. May be empty if device is not
     *                     yet claimed — auth will fail until a valid token is set.
     */
    void begin(const char* deviceName, const char* deviceToken);

    /** Stop the WS server and mDNS. */
    void stop();

    /** Call every loop() iteration. Drives WS events and deferred token refresh. */
    void loop();

    // ── State ─────────────────────────────────────────────────────────────────

    bool isEnabled()         const { return enabled_; }
    bool isListening()       const;
    bool isClientConnected() const { return connectedClient_ >= 0 && clientAuthorized_; }

    // ── Outgoing data ─────────────────────────────────────────────────────────

    /**
     * Send data to the connected WS client.
     * Serializes doc and wraps as {"type":"<type>","data":{...}}.
     * No-op if no authorized client is connected.
     */
    void publish(const char* type, JsonDocument& doc);

    /**
     * Send pre-serialized JSON to the connected WS client.
     * Wraps as {"type":"<type>","data":<jsonRaw>} without re-parsing.
     * Use for large payloads (e.g. config) to avoid double-serialize overhead.
     */
    void publish(const char* type, const char* jsonRaw, size_t len);

    // ── Callbacks ─────────────────────────────────────────────────────────────

    /** Register the shared command handler. Call before begin(). */
    void setCommandSink(CommandSink cb)              { commandSink_ = cb; }

    /** Register the token-refresh callback. Called on invalid_token. */
    void setTokenRefreshCallback(TokenRefreshCallback cb) { tokenRefreshCb_ = cb; }

    /** Update the device token (e.g. after portal auto-refresh). */
    void updateToken(const char* newToken);

private:
    char deviceName_[40];
    char deviceToken_[IDRYER_MAX_TOKEN_LEN];

    // Subclass exposes fragmented-send access to protected WebSocketsServer methods.
    class WsImpl;
    WsImpl* ws_              = nullptr;
    bool    enabled_         = false;
    int8_t  connectedClient_ = -1;
    bool    clientAuthorized_    = false;
    bool    needsTokenRefresh_   = false;

    void onWsEvent(uint8_t num, uint8_t type, uint8_t* payload, size_t length);
    void handleMessage(uint8_t num, const char* json, size_t length);

    // Internal send helpers
    void sendDoc(const char* type, JsonDocument& doc);
    void sendFragment(const char* type, const char* json, size_t len);

    CommandSink          commandSink_;
    TokenRefreshCallback tokenRefreshCb_;
};

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
