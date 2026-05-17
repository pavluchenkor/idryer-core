# 如何添加新产品

在 `idryer-core` 基础上构建新设备的实用检查清单。

两个场景：

- **最小** — 仅 MQTT + 云。足以满足大多数简单设备。
- **扩展** — MQTT + 本地 WS 访问(通过 LAN)。对于需要本地访问而不需要云的设备。

---

## 场景 1：最小的仅 MQTT 设备

最小集合：WiFi、MQTT、云状态机、一个个人资料。

参考：[`examples/minimal_mqtt_only/`](../../../examples/minimal_mqtt_only/)

### 1. 实现 IProfile

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

### 2. 组装组成根

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

### 3. 注册命令处理器并启动

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

## 场景 2：MQTT + 本地 WS 设备

扩展最小配置。添加 `LocalAccess`(LAN WebSocket + mDNS)和 `DevicePublisher` — 用于在一个调用中发布到两个传输的瘦包装器。

参考：[`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### 其他对象

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### 命令处理器 — 用于两个传输

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

### setup() 中的初始化

```cpp
s_credentials.seedSerialFromMac();
{
    idryer::DeviceIdentity identity;
    s_credentials.load(identity);
    s_local.initMdns(identity.serialNumber);   // mDNS 在 WS 启动前
    s_local.begin(identity.serialNumber, identity.token);
    s_local.setCommandSink(handleCommand);     // 同一个处理器
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

## 遥测

通过 `s_pub`(或在最小场景中直接通过 `s_mqtt`)定期发布遥测：

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

或将其包装在专用类中(示例：Storage Link 中的 `StorageTelemetryPublisher`)。

## 描述合约

添加新主题或更改有效负载时：

1. 更新 `contracts/mqtt_contract.yaml`。
2. 在 `docs/ru/` 中添加描述。

## 适用性

当前模型适用于：

- 具有云连接的独立设备(WiFi + MQTT)
- 具有本地 WS 访问(通过 LAN)的设备
- 具有 NVS 菜单的可配置设备

对于双 MCU 设备(ESP32 + RP2040) — 连接 UART 网桥(`idryer_uart.h`)。对于具有打印机集成的设备 — `idryer_integrations.h`。
