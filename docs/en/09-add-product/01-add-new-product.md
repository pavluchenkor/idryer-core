# How to add a new product

A practical checklist for building a new device on top of `idryer-core`.

Two scenarios:

- **Minimal** — MQTT + cloud only. Sufficient for most simple devices.
- **Extended** — MQTT + local WS access over LAN. For devices that need local access without the cloud.

---

## Scenario 1: Minimal MQTT-only device

Minimum set: WiFi, MQTT, cloud state machine, one profile.

Reference: [`examples/minimal_mqtt_only/`](../../../examples/minimal_mqtt_only/)

### 1. Implement IProfile

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

### 2. Assemble the composition root

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

### 3. Register the command handler and start

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
    // ... load WiFi credentials, seedSerialFromMac ...
    s_runtime.setCommandHandler(handleCommand);
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();
}
```

---

## Scenario 2: MQTT + Local WS device

Extends Minimal. Adds `LocalAccess` (LAN WebSocket + mDNS) and `DevicePublisher` — a thin wrapper for publishing to both transports in one call.

Reference: [`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### Additional objects

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### Command handler — one for both transports

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

### Initialization in setup()

```cpp
s_credentials.seedSerialFromMac();
{
    idryer::DeviceIdentity identity;
    s_credentials.load(identity);
    s_local.initMdns(identity.serialNumber);   // mDNS before WS starts
    s_local.begin(identity.serialNumber, identity.token);
    s_local.setCommandSink(handleCommand);     // same handler
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
    // product logic — sensors, telemetry via s_pub
}
```

---

## Telemetry

Periodically publish telemetry via `s_pub` (or directly via `s_mqtt` in the minimal scenario):

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

Or wrap it in a dedicated class (example: `StorageTelemetryPublisher` in Storage Link).

## Describe the contract

When adding new topics or changing payloads:

1. Update `contracts/mqtt_contract.yaml`.
2. Add a description in `docs/ru/`.

## Applicability

The current model works well for:

- Standalone devices with cloud connectivity (WiFi + MQTT)
- Devices with local WS access over LAN
- Configurable devices with NVS menu

For dual-MCU devices (ESP32 + RP2040) — connect the UART bridge (`idryer_uart.h`). For devices with printer integrations — `idryer_integrations.h`.
