# Como adicionar um novo produto

Uma lista de verificação prática para construir um novo dispositivo em cima do `idryer-core`.

Dois cenários:

- **Mínimo** — apenas MQTT + nuvem. Suficiente para a maioria dos dispositivos simples.
- **Estendido** — MQTT + acesso local WS sobre LAN. Para dispositivos que precisam de acesso local sem a nuvem.

---

## Cenário 1: Dispositivo mínimo apenas MQTT

Conjunto mínimo: WiFi, MQTT, máquina de estado na nuvem, um perfil.

Referência: [`examples/minimal_mqtt_only/`](../../../examples/minimal_mqtt_only/)

### 1. Implementar IProfile

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

### 2. Montar a raiz de composição

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

### 3. Registar o manipulador de comandos e iniciar

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

## Cenário 2: Dispositivo MQTT + Local WS

Estende Mínimo. Adiciona `LocalAccess` (LAN WebSocket + mDNS) e `DevicePublisher` — um invólucro fino para publicar em ambos os transportes em uma chamada.

Referência: [`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### Objetos adicionais

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### Manipulador de comandos — um para ambos os transportes

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

### Inicialização em setup()

```cpp
s_credentials.seedSerialFromMac();
{
    idryer::DeviceIdentity identity;
    s_credentials.load(identity);
    s_local.initMdns(identity.serialNumber);   // mDNS antes de WS iniciar
    s_local.begin(identity.serialNumber, identity.token);
    s_local.setCommandSink(handleCommand);     // mesmo manipulador
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

## Telemetria

Publique telemetria periodicamente via `s_pub` (ou diretamente via `s_mqtt` no cenário mínimo):

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

Ou envolva-o em uma classe dedicada (exemplo: `StorageTelemetryPublisher` em Storage Link).

## Descrever o contrato

Ao adicionar novos tópicos ou mudar payloads:

1. Atualize `contracts/mqtt_contract.yaml`.
2. Adicione uma descrição em `docs/ru/`.

## Aplicabilidade

O modelo atual funciona bem para:

- Dispositivos autónomos com conectividade na nuvem (WiFi + MQTT)
- Dispositivos com acesso local WS sobre LAN
- Dispositivos configuráveis com menu NVS

Para dispositivos com dois MCUs (ESP32 + RP2040) — conecte a ponte UART (`idryer_uart.h`). Para dispositivos com integrações de impressoras — `idryer_integrations.h`.
