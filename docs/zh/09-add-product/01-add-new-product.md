---
title: "如何添加基于 idryer-core 的新产品"
description: "添加新 iDryer 设备的检查清单：profile、命令、遥测、MQTT、门户支持，以及库代码和产品代码的边界。"
---

# 如何添加基于 idryer-core 的新产品

当你基于 `idryer-core` 制作新产品时使用本指南：耗材干燥箱、加热模块、照明、传感器或其他设备。它说明哪些内容应留在库中，哪些内容属于具体产品代码。

这是在 `idryer-core` 之上构建设备的实用 checklist。

两种场景：

- **Minimal** — 只有 MQTT + 云端。足以覆盖大多数简单设备。
- **Extended** — MQTT + 局域网本地 WS 访问。适用于需要不经过云端进行本地访问的设备。

---

## 场景 1：仅 MQTT 的 Minimal 设备

最小集合：WiFi、MQTT、云端状态机、一个 profile。

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

### 2. 组装组合根

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

### 3. 注册命令 handler 并启动

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

## 场景 2：MQTT + Local WS 设备

在 Minimal 基础上扩展。添加 `LocalAccess`（LAN WebSocket + mDNS）和 `DevicePublisher`，后者是一个薄封装，可在一次调用中发布到两种传输。

参考：[`examples/mqtt_with_local_ws/`](../../../examples/mqtt_with_local_ws/)

### 额外对象

```cpp
#include <local_access/local_access.h>
#include <local_access/device_publisher.h>

static idryer::LocalAccess     s_local;
static idryer::DevicePublisher s_pub(&s_mqtt, &s_local);
```

### 命令 handler — 两种传输共用一个

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

## 遙測

通过 `s_pub` 周期性发布遥测（在 minimal 场景中也可以直接通过 `s_mqtt` 发布）：

```cpp
s_pub.publishTelemetry(doc);   // → MQTT + WS
```

也可以封装到专用类中（例如 Storage Link 中的 `StorageTelemetryPublisher`）。

## 描述合同

添加新 topic 或修改 payload 时：

1. 更新 `contracts/mqtt_contract.yaml`。
2. 在 `docs/ru/` 中添加说明。

## 適用性

当前模型适合：

- 具备云端连接的独立设备（WiFi + MQTT）
- 通过 LAN 提供本地 WS 访问的设备
- 带 NVS 菜单的可配置设备

对于双 MCU 设备（ESP32 + RP2040），连接 UART 桥（`idryer_uart.h`）。对于带打印机集成的设备，使用 `idryer_integrations.h`。
