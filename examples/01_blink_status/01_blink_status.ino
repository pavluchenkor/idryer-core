// ============================================================================
//  01_blink_status — самый простой пример idryer-core.
// ============================================================================
//
// Что показывает:
//   - библиотека поднимается с минимумом кода;
//   - WiFi подключается, MQTT подключается, устройство выходит в Online;
//   - встроенный LED моргает, когда устройство онлайн.
//   Никаких датчиков, исполнителей и LAN WebSocket — только сам стек.
//
// Что обязательно настроить:
//   1. include/secrets.h в вашем PlatformIO-проекте (см. examples/secrets.h.example).
//   2. build_flags в platformio.ini:
//        -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
//        -DMQTT_USE_TLS=1
//   3. WiFi 2.4 GHz с доступом в интернет.
//
// Common pitfalls:
//   - ESP32 не работает с 5 GHz сетями.
//   - secrets.h должен лежать в include/, имя файла строго `secrets.h`.
//   - IDRYER_API_BASE — макрос со строкой; кавычки нужны и снаружи, и внутри.
//   - В loop() нельзя вызывать длинный delay() — рвёт MQTT keep-alive.
//   - Если устройство застряло в AwaitingClaim — это нормально до ввода PIN
//     в портале. Авто-claim уже включён ниже.
// ============================================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include <idryer_core.h>
#include <secrets.h>

// ── Профиль продукта (минимальный) ──────────────────────────────────────────
// IProfile — контракт между библиотекой и продуктом.
// Здесь все методы пустые: нет конфига, нет периодической логики.
class BlinkProfile : public idryer::IProfile {
public:
    void onOnline() override {}
    void loop() override {}
    void getConfig(JsonDocument& /*out*/) override {}
    bool applyConfig(int /*id*/, int /*val*/) override { return false; }

    // Публикуется в idryer/{serial}/info при первом выходе в Online.
    void buildInfoJson(char* buf, size_t len) const override {
        StaticJsonDocument<128> doc;
        doc["deviceType"]      = "blink_demo";
        doc["firmwareVersion"] = "1.0.0";
        doc["hardwareVersion"] = "1.0";
        char ts[32];
        idryer::MqttClient::getIsoTimestamp(ts);
        doc["timestamp"] = ts;
        serializeJson(doc, buf, len);
    }
};

// ── Composition root (всё статически, без new) ──────────────────────────────
static idryer::ArduinoWifiStore         s_wifiStore;
static idryer::ArduinoWifiManager       s_wifi;
static idryer::ArduinoCredentialStore   s_credentials;
static idryer::ArduinoHttpClient        s_http;
static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
static idryer::MqttClient               s_mqtt;
static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
static idryer::ActionDispatcher         s_dispatcher;
static BlinkProfile                     s_profile;
static idryer::IdryerRuntime            s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);

// ── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // HAL логирует в Serial; для Improv-сценария см. 03_with_improv.
    idryer::hal::initArduinoHal(&Serial);

    // WiFi: пробуем сохранённые credentials, иначе берём из secrets.h.
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    } else {
        s_wifiStore.save(WIFI_SSID, WIFI_PASSWORD);
        s_wifi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    // Серийный номер генерируется из MAC при первом запуске.
    s_credentials.seedSerialFromMac();

    // Авто-claim: устройство само запросит PIN, когда дойдёт до AwaitingClaim.
    s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);

    // CommandHandler не регистрируем — встроенный fallback в IdryerRuntime
    // обработает invoke/set/device.getConfig (см. 04-runtime/01-idryer-runtime.md).
    s_runtime.begin();
}

// ── loop ────────────────────────────────────────────────────────────────────
void loop() {
    s_runtime.loop();

    // LED моргает раз в 500 мс, когда устройство онлайн.
    static uint32_t lastBlink = 0;
    if (s_runtime.isOnline() && millis() - lastBlink >= 500) {
        lastBlink = millis();
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}
