# 公开 API：iDryer::Link

`iDryer::Link` 是嵌入式开发者的唯一入口点。该外观隐藏了整个 SDK 堆栈：WiFi/Improv、云状态机、HTTP 声称、MQTT、本地 WebSocket、NVS。产品只需填充 `telemetry`/`status` 字段、注册回调并调用 `begin()`/`loop()`。

---

## 生命周期

典型的 `main.cpp` 框架：

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // 仅在使用 setCommandHandler 时需要

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — 严格在 begin() 之后：begin() 安装其自己的调度程序
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## 配置：`iDryer::Config`

在 `main.cpp` 中填充一次，传递给 `Link` 构造函数。所有字段使用聚合初始化（C++ 指定初始化器）。

| 字段 | 类型 | 目的 | 注意 |
|------|------|------|------|
| `deviceType` | `DeviceType` | 设备类型 | **必需** |
| `unitsCount` | `uint8_t` | 单元（舱室）数量，1..`MAX_UNITS` (4) | **必需** |
| `hasAirTemp` | `bool` | 空气温度传感器存在 | false = 字段从 JSON 中省略 |
| `hasAirHumidity` | `bool` | 湿度传感器存在 | false = 字段从 JSON 中省略 |
| `hasHeaterTemp` | `bool` | 加热器温度传感器存在 | — |
| `hasHeaterPower` | `bool` | 加热器功率传感器存在 | — |
| `hasFanStatus` | `bool` | 风扇状态存在 | — |
| `hasScales` | `bool` | 秤存在 | — |
| `hasRfid` | `bool` | RFID 读者存在 | — |
| `allowHa` | `bool` | 允许 Home Assistant 集成 | false = SDK 不创建客户端 |
| `allowBambu` | `bool` | 允许 Bambu Lab LAN 集成 | — |
| `allowMoonraker` | `bool` | 允许 Moonraker/Klipper 集成 | — |
| `telemetryPeriodMs` | `uint32_t` | `Telemetry` 的自动发布周期（毫秒） | 0 = 不发布 |
| `statusPeriodMs` | `uint32_t` | `Status` 的自动发布周期（毫秒） | 0 = 不发布 |
| `hardwareVersion` | `const char*` | 硬件版本字符串 | **必需** |
| `firmwareVersion` | `const char*` | 固件版本字符串 | **必需** |

---

## 类 `iDryer::Link`

### 构造函数

```cpp
explicit Link(const Config& cfg);
```

通过 const 引用获取配置。`CFG` 必须存在于整个对象的生命周期内（通常是 `static const`）。

### 方法

#### `begin()`

```cpp
bool begin();
```

启动整个 SDK 堆栈：WiFi/Improv、云状态机、HTTP 声称、MQTT、本地 WebSocket、NVS 持久化。

在 `setup()` 中调用一次。初始化成功时返回 `true`。

```cpp
void setup() {
    link.begin();
}
```

#### `loop()`

```cpp
void loop();
```

唯一所需的滴答。为 WiFi/MQTT/LocalAccess 提供服务，并在其计时器上自动发布遥测和状态。

在 `loop()` 的每次迭代中调用。没有这个调用，连接不会被维护。

```cpp
void loop() {
    link.loop();  // 在 loop() 中第一个，在产品逻辑之前
}
```

*来源：`iDryer-Storage/src/main.cpp:253`、`iHeater-link/src/main.cpp:381`。*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

立即发布 `link.telemetry` 的当前状态，不管 `telemetryPeriodMs` 计时器。

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

立即发布 `link.status` 的当前状态。处理命令后使用，当新状态必须立即在门户中反映时。

```cpp
// iHeater-link/src/main.cpp:238
device().publishStatusNow();
```

#### `raiseEvent()`

```cpp
void raiseEvent(EventKind   severity,
                const char* event,
                const char* message,
                uint8_t     unitId = 0xFF);
```

发布事件到主题 `idryer/{serial}/events`。立即发送。

| 参数 | 类型 | 目的 |
|------|------|------|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | 事件代码，例如 `"OVERHEAT"`、`"SESSION_COMPLETE"` |
| `message` | `const char*` | 任意调试文本 |
| `unitId` | `uint8_t` | 单元索引 (0..unitsCount-1) 或 `0xFF` 表示设备范围 |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

为通过 MQTT 或 Local WS 到达的业务命令（`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`）注册回调。命令源是透明的。

`RequestCallback` = `std::function<void(const iDryer::Request&)>`

```cpp
link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start: myStart(r.unitId, r.targetTempC); break;
        case iDryer::RequestKind::Stop:  myStop(r.unitId);                 break;
        default: break;
    }
});
```

**重要：** 如果设置了 `runtime()->setCommandHandler(...)`，此回调不会被调用——完整的调度程序拦截所有命令。

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

为 `commands/profile` 注册回调——多步骤干燥计划。

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

当集成连接状态更改时调用（HA、Bambu、Moonraker）。可选回调。

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

当云声称流返回用于在门户中输入的 PIN 时调用。

`ClaimPinCallback` = `std::function<void(const char* pin, uint32_t expiresInSeconds)>`

```cpp
// iHeater-link/src/main.cpp:367
device().onClaimPin([](const char* pin, uint32_t expiresInSeconds) {
    Serial.printf("CLAIM_PIN:%s:%u\n", pin, expiresInSeconds);
});
```

#### `isOnline()`

```cpp
bool isOnline() const;
```

如果设备已注册且 MQTT 会话处于活动状态，则返回 `true`。

```cpp
// iHeater-link/src/main.cpp:281
if (device().isOnline()) { ... }
```

#### `serial()`

```cpp
const char* serial() const;
```

设备序列号（来自 NVS 的字符串，在声称期间分配）。声称完成前为空字符串。

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

仅在 NVS 中尚未设置凭据时写入 WiFi 凭据。在 `begin()` 之前调用。在带有硬编码凭据的开发环境中使用。

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

始终覆盖 NVS 中的 WiFi 凭据。开发助手和强制重新配置。

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

手动启动云声称流程（配置→注册→检查声称）。成功时调用已注册的 `onClaimPin` 回调。如果请求被接受，返回 `true`。

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

从 NVS 中移除设备令牌并重新启动芯片。重新启动后，设备未声称——自动声称流程再次启动。此函数不返回。

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

集成管理器的出口——用于产品侧接线（Moonraker 舱目标回调、Bambu 打印机状态等）。

需要 `#include <integrations/common/link_integrations_manager.h>`。

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

SDK MQTT 客户端的出口——用于发布自己的主题或集成到命令路由中的组件（例如 `MenuBridge`）。

需要 `#include <mqtt/mqtt_client.h>`。

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

双重发布助手的出口——同时向 MQTT 和 Local WS 发送一个有效负载。对于必须与自动发布遥测相同方式到达 LAN 客户端的产品响应使用。

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

SDK 运行时的出口——用于设置完整的命令处理程序而不是外观调度程序。设置 `setCommandHandler(...)` 后，外观的 `onRequest`/`onProfile` 不再通过 MQTT 路径调用。

**重要：** 严格在 `begin()` 之后调用——`begin()` 安装其自己的调度程序，必须被覆盖。

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// 处理程序签名：
// void handleCommand(const char* cmd, JsonObjectConst data);
```

需要 `#include <runtime/idryer_runtime.h>`。

---

### 遥测字段 {#telemetry-fields}

由产品在 `loop()` 中填充。SDK 在 `telemetryPeriodMs` 计时器上读取它们并发布到 MQTT 和 Local WS。

| 字段 | 类型 | 配置标志 | 目的 |
|------|------|---------|------|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | 空气温度，°C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | 湿度，% |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | 加热器温度，°C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | 加热器功率，0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | 风扇状态 |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | 重量，克 |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

`unitId` = 第一个（或唯一的）单元为 0。索引必须 < `Config.unitsCount`。

`Status` 字段——相同的结构，但用于操作状态：

| 字段 | 类型 | 目的 |
|------|------|------|
| `status.mode[unitId]` | `UnitMode` | 当前单元模式 |
| `status.targetTempC[unitId]` | `float` | 目标温度 |
| `status.durationS[unitId]` | `uint32_t` | 请求的持续时间，s（0 = 无限） |
| `status.elapsedS[unitId]` | `uint32_t` | 会话开始以来经过的时间，s |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### 通过运行时的回调注册

如果需要对传入命令的完全控制（例如，产品处理 `get_config`、`set`、非标准 `invoke`）：

```cpp
// 签名——来自 idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// 注册——严格在 link.begin() 之后
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — 命令字符串（`"set"`、`"invoke"`、`"ping"`、`"get_config"`）。
`data` — ArduinoJson `JsonObjectConst` 与有效负载。

使用这种方法，`onRequest()` 和 `onProfile()` 不从 MQTT 路径调用——产品直接处理命令。

---

## 枚举

### `iDryer::DeviceType`

| 值 | 数字 | 目的 |
|-----|------|------|
| `Unknown` | 0 | 无 / 未定义 |
| `Dryer` | 1 | 干衣机（iDryer LINK） |
| `Heater` | 2 | 加热器 |
| `StorageLink` | 4 | 存储链接（ESP32-C3 + LED） |
| `IHeaterLink` | 5 | iHeater 链接 |

### `iDryer::UnitMode`

`Idle`、`Drying`、`Storage`、`Profile`、`Fault`、`Unknown`

### `iDryer::EventKind`

`Info`、`Warning`、`Error`

### `iDryer::RequestKind`

`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`

### `iDryer::IntegrationKind`

`Ha`、`Bambu`、`Moonraker`

### `iDryer::IntegrationState`

`Disabled`、`Idle`、`Connecting`、`Online`、`ConfigMissing`、`Error`

---

## 何时深入

外观对于大多数任务来说是足够的。如果需要在外观级别下方工作——使用 `idryer::IdryerRuntime`、`idryer::MqttClient`、`idryer::cloud::LinkIntegrationsManager` ——请参阅架构部分。
