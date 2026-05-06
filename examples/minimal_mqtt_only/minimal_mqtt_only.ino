// ============================================================================
//  minimal_mqtt_only — устройство с собственным обработчиком команд (без LAN WS).
// ============================================================================
//
// Что показывает:
//   - composition root c IProfile;
//   - регистрация продуктового handleCommand через runtime.setCommandHandler();
//   - обработка commands/invoke и commands/set через ActionDispatcher;
//   - ответ на device.getConfig.
//   Без LocalAccess, без DevicePublisher, без датчиков.
//
// Когда читать:
//   - после 01_blink_status, перед 03_with_improv и mqtt_with_local_ws.
//
// Что обязательно настроить:
//   1. include/secrets.h: WIFI_SSID, WIFI_PASSWORD (см. examples/secrets.h.example).
//   2. build_flags в platformio.ini:
//        -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
//        -DMQTT_USE_TLS=1
//
// Common pitfalls:
//   - Если зарегистрировать setCommandHandler — встроенный fallback
//     IdryerRuntime отключается. Все invoke/set теперь должны явно
//     перенаправляться в s_dispatcher.handleInvoke / handleSet (см. ниже).
//   - setInvokeHandler / setSetCallback — это plain function pointer + ctx.
//     Лямбда без захвата конвертируется автоматически; с захватом — нет.
//   - setCommandHandler() обязательно вызывать ДО runtime.begin().
//   - ping приходит как commands/ping и обрабатывается рантаймом сам,
//     ваш handleCommand его не увидит.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <idryer_core.h>
#include <secrets.h>

// ── Реализация IProfile ─────────────────────────────────────────────────────
// Замените содержимое методов на вашу продуктовую логику.
class MyProfile : public idryer::IProfile {
public:
    void onOnline() override {
        // Вызывается один раз при первом выходе в Online.
        // Здесь обычно загружают конфиг из NVS и применяют к железу.
    }

    void loop() override {
        // Вызывается каждую итерацию IdryerRuntime::loop().
        // Здесь — таймеры, опрос датчиков, актуаторы.
    }

    void getConfig(JsonDocument& out) override {
        // Заполняет doc текущим конфигом для публикации в config-топик.
        out["param1"] = 42;
    }

    bool applyConfig(int id, int val) override {
        // Применить параметр из commands/set. id — обычно из menu_ids.h.
        (void)id; (void)val;
        return true;
    }

    void buildInfoJson(char* buf, size_t len) const override {
        // Публикуется в idryer/{serial}/info при выходе в Online и при ping.
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

// ── Обработчик команд ───────────────────────────────────────────────────────
// Единая точка входа для всех команд (кроме ping, который обрабатывает рантайм).
static void handleCommand(const char* cmd, JsonObjectConst data) {
    const char* action = data["action"] | "";

    // get_config — отвечаем текущим конфигом устройства.
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))
    {
        StaticJsonDocument<256> doc;
        s_profile.getConfig(doc);
        s_mqtt.publishConfig(doc);
        return;
    }

    // invoke — действия (led.pulse, my.action, …) → ActionDispatcher.
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }

    // set — изменение параметра конфига → ActionDispatcher.
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }

    // Здесь добавляйте свои продуктовые команды:
    // if (strcmp(cmd, "my_command") == 0) { ... return; }
}

// ── Колбэки ActionDispatcher ───────────────────────────────────────────────
// Вызываются из handleInvoke / handleSet выше.
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    // Здесь должен быть диспатч в актуатор по имени action.
    // Пример: return s_myExecutor.execute(action, args);
    (void)action; (void)args;
    return false;
}

static void onSetCommand(JsonObjectConst data, void* /*ctx*/) {
    // commands/set приходит как {"id": N, "val": V}.
    int id  = data["id"]  | -1;
    int val = data["val"] | -1;
    if (id < 0 || val < 0) return;
    s_profile.applyConfig(id, val);
}

// ── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    idryer::hal::initArduinoHal(&Serial);

    // WiFi: сохранённые credentials имеют приоритет над WIFI_SSID из secrets.h.
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    } else {
        s_wifiStore.save(WIFI_SSID, WIFI_PASSWORD);
        s_wifi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    // Серийник из MAC, если NVS пустой.
    s_credentials.seedSerialFromMac();

    // Авто-claim для standalone-устройства.
    s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);

    // Колбэки ActionDispatcher (до setCommandHandler / runtime.begin).
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    // Регистрируем handleCommand — теперь рантайм отдаёт нам всё, кроме ping.
    s_runtime.setCommandHandler(handleCommand);
    s_runtime.begin();
}

// ── loop ────────────────────────────────────────────────────────────────────
void loop() {
    s_runtime.loop();
    // Здесь — продуктовая периодика: датчики, телеметрия, актуаторы.
}
