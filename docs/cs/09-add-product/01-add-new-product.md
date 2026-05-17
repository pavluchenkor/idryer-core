# Jak přidat nový produkt

Praktický kontrolní seznam pro vytvoření nového zařízení nad `idryer-core`.

Dva scénáře:

- **Minimální** — pouze MQTT + cloud. Postačuje pro většinu jednoduchých zařízení.
- **Rozšířený** — MQTT + místní WS přístup přes LAN. Pro zařízení, která potřebují místní přístup bez cloudu.

---

## Scénář 1: Minimální zařízení MQTT

Minimální sada: WiFi, MQTT, stavový stroj cloudu, jeden profil.

Odkaz: [`examples/minimal_mqtt_only/`](../../../examples/minimal_mqtt_only/)

### 1. Implementujte IProfile

```cpp
// src/mydevice/my_profile.h
#include <profiles/IProfile.h>

class MyProfile : public idryer::IProfile {
public:
    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;
};
```

### 2. Sestavte kořen kompozice

```cpp
#include <idryer_core.h>

static idryer::ArduinoWifiStore       s_wifiStore;
static idryer::ArduinoWifiManager     s_wifi;
static idryer::ArduinoCredentialStore s_credentials;
static idryer::ArduinoHttpClient      s_http;

static idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
static idryer::MqttClient               s_mqtt;
static idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
static idryer::ActionDispatcher         s_dispatcher;

static MyProfile             s_profile;
static idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
```

### 3. Zaregistrujte obsluhu příkazů a spusťte

```cpp
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
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
}

void setup() {
    Serial.begin(115200);
    idryer::hal::initArduinoHal(&Serial);
    // ... načtěte přihlašovací údaje WiFi, seedSerialFromMac ...
    s_runtime.setCommandHandler(handleCommand);
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();
}
```

---

## Scénář 2: MQTT + Místní WS zařízení

Rozšiřuje Minimální. Přidává `LocalAccess` (LAN WebSocket + mDNS) a `DevicePublisher` — tenkého obalu pro publikování na oba přenosy v jednom volání.

Odkaz: [`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### Dodatečné objekty

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### Obsluha příkazů — jeden pro oba přenosy

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    const char* action = data["action"] | "";
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(action, "device.getConfig") == 0))
    {
        StaticJsonDocument<256> doc;
        s_profile.getConfig(doc);
        s_pub.publishConfig(doc);   // → MQTT + WS
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
}
```

### Inicializace v setup()

```cpp
s_credentials.seedSerialFromMac();
{
    idryer::DeviceIdentity identity;
    s_credentials.load(identity);
    s_local.initMdns(identity.serialNumber);   // mDNS před spuštěním WS
    s_local.begin(identity.serialNumber, identity.token);
    s_local.setCommandSink(handleCommand);     // stejná obsluha
    s_local.setTokenRefreshCallback([]() {
        idryer::DeviceIdentity id;
        s_credentials.load(id);
        s_local.updateToken(id.token);
    });
}
s_runtime.setCommandHandler(handleCommand);
s_runtime.begin();
```

### loop()

```cpp
void loop() {
    s_runtime.loop();
    s_local.loop();
    // logika produktu — senzory, telemetrie přes s_pub
}
```

---

## Telemetrie

Pravidelně publikujte telemetrii přes `s_pub` (nebo přímo přes `s_mqtt` v minimálním scénáři):

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

Nebo ji zabalte do dedikované třídy (příklad: `StorageTelemetryPublisher` v Storage Link).

## Popište smlouvu

Při přidávání nových témat nebo změně datových částí:

1. Aktualizujte `contracts/mqtt_contract.yaml`.
2. Přidejte popis do `docs/ru/`.

## Použitelnost

Aktuální model funguje dobře pro:

- Samostatná zařízení s cloudovým připojením (WiFi + MQTT)
- Zařízení s místním WS přístupem přes LAN
- Konfigurovatelná zařízení s NVS nabídkou

Pro zařízení se dvěma MCU (ESP32 + RP2040) — připojte most UART (`idryer_uart.h`). Pro zařízení s integracemi tiskárny — `idryer_integrations.h`.
