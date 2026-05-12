// ============================================================================
// SCAFFOLD: storage_link
// Generated 2026-05-12 by contracts/gen_scaffold.py from mqtt_contract.yaml
//
// HOW TO START:
//   1. Copy this directory to your PlatformIO project root.
//   2. Copy include/secrets.h.example → include/secrets.h, fill WiFi credentials.
//   3. Fill in the TODO sections below with your hardware logic.
//   4. Run: pio run -e storage_link-prod
//   5. Flash, connect Improv (or use hardcoded SSID), claim on portal.idryer.org.
//
// Capabilities: led, weight, rfid
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <idryer_core.h>
#include <secrets.h>

// ── Config ────────────────────────────────────────────────────────────────────
// Flags generated from device_profiles.storage_link in mqtt_contract.yaml.
// After adding a new capability: edit mqtt_contract.yaml → run contracts/regen.sh.
static const idryer::Config CFG = {
    .deviceType        = idryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    // Peripheral capabilities:
    .hasLed                 = true,   // Адресная LED-лента
    .hasScales              = true,   // Весовой датчик (граммы филамента)
    .hasRfid                = true,   // RFID-ридер метки катушки
    .hasHeaterPower         = false,  // (not in this profile)
    .hasFanStatus           = false,  // (not in this profile)
    // Basic air sensors (set true if your hardware has them):
    .hasAirTemp        = false,  // TODO: SHT31, DHT22, etc.
    .hasAirHumidity    = false,
    .hasHeaterTemp     = false,
    // Cloud integrations (set true if you need them):
    .allowBambu        = false,
    .allowMoonraker    = false,
    .allowHa           = false,
    // Publish periods (ms):
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    // Identity shown on portal:
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "0.1.0",
    .model             = "storage_link",
};

// ── IProfile implementation ──────────────────────────────────────────────────
class StorageLinkProfile : public idryer::IProfile {
public:
    void onOnline() override {
        // Called once when device reaches Online state.
        // TODO: load config from NVS, apply to hardware (pins, PWM, etc.).
    }

    void loop() override {
        // Called every IdryerRuntime::loop().
        // TODO: read sensors, fill telemetry, call s_runtime.publishTelemetry()
        //   static uint32_t t = 0;
        //   if (millis() - t > CFG.telemetryPeriodMs) {
        //       t = millis();
        //       idryer::Telemetry tel = {};
        //       tel.airTempC[0]       = myTempSensor.read();
        //       tel.airHumidityPct[0] = myHumSensor.read();
        //       s_runtime.publishTelemetry(tel);
        //   }
    }

    void getConfig(JsonDocument& out) override {
        // Snapshot of current config → published to idryer/{serial}/config.
        // TODO: serialize your menu/NVS state here.
        out["v"] = 1;
    }

    bool applyConfig(int id, int val) override {
        // Apply parameter from commands/set (id = menu item id, val = new value).
        // TODO: switch(id) { case MENU_ID_BRIGHTNESS: applyBrightness(val); break; }
        (void)id; (void)val;
        return true;
    }

    void buildInfoJson(char* buf, size_t len) const override {
        // Published to idryer/{serial}/info → portal reads capabilities,
        // builds DynamicCard widgets automatically.
        StaticJsonDocument<512> doc;
        doc["deviceType"]      = "storage_link";
        doc["firmwareVersion"] = CFG.firmwareVersion;
        doc["hardwareVersion"] = CFG.hardwareVersion;
        JsonObject caps = doc.createNestedObject("capabilities");
        caps["led"] = true;
        caps["weight"] = true;
        caps["rfid"] = true;
        char ts[32];
        idryer::MqttClient::getIsoTimestamp(ts);
        doc["timestamp"] = ts;
        serializeJson(doc, buf, len);
    }
};

// ── Platform layer ────────────────────────────────────────────────────────────
static idryer::ArduinoWifiStore       s_wifiStore;
static idryer::ArduinoWifiManager     s_wifi;
static idryer::ArduinoCredentialStore s_credentials;
static idryer::ArduinoHttpClient      s_http;

// ── Cloud stack ───────────────────────────────────────────────────────────────
static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
static idryer::MqttClient               s_mqtt;
static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
static idryer::ActionDispatcher         s_dispatcher;

// ── Product layer ─────────────────────────────────────────────────────────────
static StorageLinkProfile   s_profile;
static idryer::IdryerRuntime  s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);

// ── Command handler ──────────────────────────────────────────────────────────
static void handleCommand(const char* cmd, JsonObjectConst data) {
    const char* action = data["action"] | "";

    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))
    {
        StaticJsonDocument<256> doc;
        s_profile.getConfig(doc);
        s_mqtt.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set")    == 0) { s_dispatcher.handleSet(data);    return; }
    // TODO: add product-specific commands here:
    // if (strcmp(cmd, "my_command") == 0) { ... return; }
}

// ── ActionDispatcher callbacks ────────────────────────────────────────────────
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    // Actions available for this profile (from invoke_actions in mqtt_contract.yaml):
    //   led.pulse  (args: ledIndex, ledCount, animation, color, durationSec)  — Универсальная команда управления LED-лентой Storage Link.
    // Example dispatch:
    // if (strcmp(action, "led.pulse") == 0) { /* TODO */ return true; }
    (void)action; (void)args;
    return false;
}

static void onSet(JsonObjectConst data, void* /*ctx*/) {
    int id  = data["id"]  | -1;
    int val = data["val"] | -1;
    if (id < 0 || val < 0) return;
    s_profile.applyConfig(id, val);
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    idryer::hal::initArduinoHal(&Serial);

    // WiFi: NVS credentials take priority over secrets.h defaults.
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    } else {
        s_wifiStore.save(WIFI_SSID, WIFI_PASSWORD);
        s_wifi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    s_credentials.seedSerialFromMac();
    s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);

    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSet, nullptr);

    s_runtime.setCommandHandler(handleCommand);
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();
}
