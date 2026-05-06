# Как добавить новый продукт

Практический чеклист для сборки нового устройства на базе `idryer-core`.

Два сценария:

- **Minimal** — только MQTT + облако. Достаточно для большинства простых устройств.
- **Extended** — MQTT + локальный WS-доступ по LAN. Для устройств, которым нужен локальный доступ без облака.

---

## Сценарий 1: Minimal MQTT-only device

Минимальный набор: WiFi, MQTT, облачная стейт-машина, один профиль.

Референс: [`examples/minimal_mqtt_only/`](../../../examples/minimal_mqtt_only/)

### 1. Реализовать IProfile

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

### 2. Собрать composition root

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

### 3. Зарегистрировать command handler и запустить

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

## Сценарий 2: MQTT + Local WS device

Расширяет Minimal. Добавляет `LocalAccess` (LAN WebSocket + mDNS) и `DevicePublisher` — тонкую обёртку для публикации в оба транспорта одним вызовом.

Референс: [`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### Дополнительные объекты

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### Command handler — один для обоих транспортов

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

### Инициализация в setup()

```cpp
s_credentials.seedSerialFromMac();
{
    idryer::DeviceIdentity identity;
    s_credentials.load(identity);
    s_local.initMdns(identity.serialNumber);   // mDNS до старта WS
    s_local.begin(identity.serialNumber, identity.token);
    s_local.setCommandSink(handleCommand);     // тот же handler
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
    // продуктовая логика — сенсоры, телеметрия через s_pub
}
```

---

## Telemetry

Периодически публиковать телеметрию через `s_pub` (или напрямую через `s_mqtt` в minimal-сценарии):

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

Или обернуть в отдельный класс (пример: `StorageTelemetryPublisher` в Storage Link).

## Описать контракт

При добавлении новых топиков или изменении payload:

1. Обновить `contracts/mqtt_contract.yaml`.
2. Добавить описание в `docs/ru/`.

## Область применимости

Текущая модель хорошо подходит для:

- Standalone-устройств с облачным подключением (WiFi + MQTT)
- Устройств с локальным WS-доступом по LAN
- Конфигурируемых устройств с NVS-меню

Для двухпроцессорных устройств (ESP32 + RP2040) — подключить UART-бридж (`idryer_uart.h`). Для устройств с интеграциями принтеров — `idryer_integrations.h`.
