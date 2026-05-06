// ============================================================================
//  mqtt_with_local_ws — MQTT + локальный LAN WebSocket-сервер.
// ============================================================================
//
// Что показывает:
//   - LocalAccess: WS-сервер на порту 81 + mDNS (_idryer._tcp);
//   - DevicePublisher — dual-publish helper (MQTT + LAN WS одной строкой);
//   - один handleCommand используется обоими transport'ами (MQTT и WS).
//
// Когда читать:
//   - после minimal_mqtt_only и 03_with_improv. Это самый сложный из примеров.
//
// Что обязательно настроить:
//   1. include/secrets.h: WIFI_SSID, WIFI_PASSWORD.
//   2. build_flags: -DIDRYER_API_BASE='"…"' и -DMQTT_USE_TLS=1.
//   3. Установить библиотеку WebSockets (Markus Sattler / links2004).
//
// Common pitfalls:
//   - LocalAccess поддерживает ОДНОГО клиента одновременно. Второй WS-клиент
//     получит отказ.
//   - Первое сообщение WS-клиента должно быть {"type":"auth","token":"<token>"}.
//   - initMdns() зовите как только серийник известен — раньше WS-сервера,
//     чтобы приложение могло обнаружить устройство сразу после WiFi.
//   - DevicePublisher НЕ проверяет, кто из транспортов подключён, — методы
//     publishX тихо ничего не делают, если транспорт не готов. Это нормально.
//   - setCommandSink(handleCommand) — тот же указатель, что в
//     setCommandHandler(). Один обработчик, два транспорта.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <idryer_core.h>
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>
#include <secrets.h>

// ── Реализация IProfile ─────────────────────────────────────────────────────
class MyProfile : public idryer::IProfile {
public:
    void onOnline() override {}
    void loop() override {}

    void getConfig(JsonDocument& out) override {
        out["param1"] = 42;
    }

    bool applyConfig(int /*id*/, int /*val*/) override {
        return true;
    }

    void buildInfoJson(char* buf, size_t len) const override {
        StaticJsonDocument<128> doc;
        doc["deviceType"]      = "my_device";
        doc["firmwareVersion"] = "1.0.0";
        doc["hardwareVersion"] = "1.0";
        char ts[32];
        idryer::MqttClient::getIsoTimestamp(ts);
        doc["timestamp"] = ts;
        serializeJson(doc, buf, len);
    }
};

// ── Платформенный слой ──────────────────────────────────────────────────────
static idryer::ArduinoWifiStore       s_wifiStore;
static idryer::ArduinoWifiManager     s_wifi;
static idryer::ArduinoCredentialStore s_credentials;
static idryer::ArduinoHttpClient      s_http;

// ── Облачный стек ───────────────────────────────────────────────────────────
static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
static idryer::MqttClient               s_mqtt;
static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
static idryer::ActionDispatcher         s_dispatcher;

// ── Продуктовый слой ────────────────────────────────────────────────────────
static MyProfile             s_profile;
static idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);

// ── Локальный transport: LAN WebSocket + mDNS ───────────────────────────────
static idryer::LocalAccess s_local;

// ── Dual-publish helper: MQTT + LAN WS одной строкой ───────────────────────
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);

// ── Обработчик команд ───────────────────────────────────────────────────────
// Один и тот же handleCommand используется обоими transport'ами:
//   MQTT   → IdryerRuntime → handleCommand
//   LAN WS → LocalAccess   → handleCommand (через setCommandSink)
static void handleCommand(const char* cmd, JsonObjectConst data) {
    const char* action = data["action"] | "";

    // get_config — отвечаем в оба транспорта (s_pub дублирует MQTT и WS).
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))
    {
        StaticJsonDocument<256> doc;
        s_profile.getConfig(doc);
        s_pub.publishConfig(doc);
        return;
    }

    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }

    // Здесь добавляйте продуктовые команды.
}

// ── Колбэки ActionDispatcher ───────────────────────────────────────────────
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    (void)action; (void)args;
    return false;
}

static void onSetCommand(JsonObjectConst data, void* /*ctx*/) {
    int id  = data["id"]  | -1;
    int val = data["val"] | -1;
    if (id < 0 || val < 0) return;
    s_profile.applyConfig(id, val);
}

// ── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    idryer::hal::initArduinoHal(&Serial);

    // WiFi.
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    } else {
        s_wifiStore.save(WIFI_SSID, WIFI_PASSWORD);
        s_wifi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    s_credentials.seedSerialFromMac();

    // LocalAccess: mDNS до старта WS-сервера, чтобы приложение могло
    // обнаружить устройство сразу после подключения к WiFi.
    {
        idryer::DeviceIdentity identity;
        s_credentials.load(identity);
        s_local.initMdns(identity.serialNumber);
        s_local.begin(identity.serialNumber, identity.token);
        s_local.setCommandSink(handleCommand);          // тот же handler

        // Если WS-клиент пришёл с устаревшим токеном — перечитываем NVS
        // и обновляем токен в LocalAccess.
        s_local.setTokenRefreshCallback([]() {
            idryer::DeviceIdentity id;
            s_credentials.load(id);
            s_local.updateToken(id.token);
        });
    }

    s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);

    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    s_runtime.setCommandHandler(handleCommand);
    s_runtime.begin();
}

// ── loop ────────────────────────────────────────────────────────────────────
void loop() {
    s_runtime.loop();
    s_local.loop();

    // Пример периодической телеметрии (раскомментируйте, когда есть данные):
    // static uint32_t lastTm = 0;
    // if (millis() - lastTm >= 10000) {
    //     lastTm = millis();
    //     StaticJsonDocument<128> doc;
    //     JsonArray units = doc.createNestedArray("units");
    //     JsonObject u    = units.createNestedObject();
    //     u["unitId"]      = "U1";
    //     u["temperature"] = 23.5f;
    //     u["humidity"]    = 47.2f;
    //     s_pub.publishTelemetry(doc);   // → MQTT + WS
    // }
}
