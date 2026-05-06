// ============================================================================
//  03_with_improv — provisioning WiFi через Improv (без хардкода SSID/пароля).
// ============================================================================
//
// Что показывает:
//   - WiFi-credentials получаются от Improv-клиента (приложение/web) по Serial;
//   - правильный порядок передачи Serial: сначала Improv, потом HAL-логи;
//   - сохранение credentials в NVS, восстановление при следующем boot.
//
// Чем отличается от 01_blink_status:
//   - не требует WIFI_SSID/WIFI_PASSWORD в secrets.h;
//   - но всё ещё требует IDRYER_API_BASE (через build_flags в platformio.ini).
//
// Что обязательно настроить:
//   1. build_flags в platformio.ini:
//        -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
//        -DMQTT_USE_TLS=1
//   2. Подключить библиотеку Improv WiFi (jnthas/Improv-WiFi-Library).
//   3. Указать корректный ChipFamily ниже (CF_ESP32_C3, CF_ESP32_S3, …).
//
// Common pitfalls:
//   - HAL_LOG_* нельзя писать в Serial, пока Improv его слушает — ломает чек-сумму.
//     Поэтому в setup() сначала initArduinoHal(nullptr), и только после WiFi
//     возвращаем Serial в HAL: initArduinoHal(&Serial).
//   - Improv общается на 115200; не меняйте baudrate в setup().
//   - ChipFamily должен совпадать с реальным чипом, иначе клиент Improv не
//     распознает устройство.
//   - Если в NVS уже есть credentials — Improv можно не использовать, устройство
//     подключится к сохранённой сети сразу.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ImprovWiFiLibrary.h>
#include <idryer_core.h>

// ── Профиль продукта (минимальный) ──────────────────────────────────────────
class ImprovProfile : public idryer::IProfile {
public:
    void onOnline() override {}
    void loop() override {}
    void getConfig(JsonDocument& /*out*/) override {}
    bool applyConfig(int /*id*/, int /*val*/) override { return false; }

    void buildInfoJson(char* buf, size_t len) const override {
        StaticJsonDocument<128> doc;
        doc["deviceType"]      = "improv_demo";
        doc["firmwareVersion"] = "1.0.0";
        doc["hardwareVersion"] = "1.0";
        char ts[32];
        idryer::MqttClient::getIsoTimestamp(ts);
        doc["timestamp"] = ts;
        serializeJson(doc, buf, len);
    }
};

// ── Composition root ────────────────────────────────────────────────────────
static idryer::ArduinoWifiStore         s_wifiStore;
static idryer::ArduinoWifiManager       s_wifi;
static idryer::ArduinoCredentialStore   s_credentials;
static idryer::ArduinoHttpClient        s_http;
static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
static idryer::MqttClient               s_mqtt;
static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
static idryer::ActionDispatcher         s_dispatcher;
static ImprovProfile                    s_profile;
static idryer::IdryerRuntime            s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);

// ── Improv ──────────────────────────────────────────────────────────────────
static ImprovWiFi s_improv(&Serial);
static bool       s_logsEnabled = false;   // HAL включается только после connect

// Improv передал нам SSID/пароль.
static void onImprovConnected(const char* ssid, const char* password) {
    s_wifiStore.save(ssid, password);
    s_wifi.begin(ssid, password);
}

// ── setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Пока Serial отдан Improv — логи в /dev/null.
    idryer::hal::initArduinoHal(nullptr);

    s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3,
                           "Improv demo", "1.0.0", "ImprovDemo");
    s_improv.onImprovConnected(onImprovConnected);

    // Если в NVS уже есть credentials — подключаемся сразу, Improv не нужен.
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    }

    s_credentials.seedSerialFromMac();
    s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
    s_runtime.begin();
}

// ── loop ────────────────────────────────────────────────────────────────────
void loop() {
    s_runtime.loop();

    if (!s_logsEnabled) {
        // Пока WiFi не подключён — слушаем Improv-протокол на Serial.
        s_improv.handleSerial();
        if (WiFi.status() == WL_CONNECTED) {
            // WiFi есть — Serial свободен, можно включать HAL-логи.
            s_logsEnabled = true;
            idryer::hal::initArduinoHal(&Serial);
            Serial.println("\n[BOOT] WiFi connected, Improv done");
            Serial.printf("[BOOT] IP: %s  RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
    }
}
