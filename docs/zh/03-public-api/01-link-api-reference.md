# 公開 API：iDryer::Link

`iDryer::Link` 是嵌入式開發人員的唯一進入點。此外觀隱藏整個 SDK 棧：WiFi/Improv、雲狀態機、HTTP 聲明、MQTT、本地 WebSocket、NVS。產品只需填充 `telemetry`/`status` 字段、註冊回調並調用 `begin()`/`loop()`。

---

## 生命週期

典型的 `main.cpp` 骨架：

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // only needed if setCommandHandler is used

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
    // setCommandHandler — strictly AFTER begin(): begin() installs its own dispatcher
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

在 `main.cpp` 中填充一次，并传给 `Link` 构造函数。所有字段都使用聚合初始化（C++ 指定初始化器）。

| 字段 | 类型 | 用途 | 备注 |
|-------|------|---------|------|
| `deviceType` | `DeviceType` | 设备类型 | **必填** |
| `unitsCount` | `uint8_t` | 单元（腔体）数量，1..`MAX_UNITS` (4) | **必填** |
| `hasAirTemp` | `bool` | 是否有空气温度传感器 | false = JSON 中省略字段 |
| `hasAirHumidity` | `bool` | 是否有湿度传感器 | false = JSON 中省略字段 |
| `hasHeaterTemp` | `bool` | 是否有加热器温度传感器 | — |
| `hasHeaterPower` | `bool` | 是否有加热功率传感器 | — |
| `hasFanStatus` | `bool` | 是否有风扇状态 | — |
| `hasScales` | `bool` | 是否有称重 | — |
| `hasRfid` | `bool` | 是否有 RFID 读卡器 | — |
| `allowHa` | `bool` | 允许 Home Assistant 集成 | false = SDK 不创建客户端 |
| `allowBambu` | `bool` | 允许 Bambu Lab LAN 集成 | — |
| `allowMoonraker` | `bool` | 允许 Moonraker/Klipper 集成 | — |
| `telemetryPeriodMs` | `uint32_t` | `Telemetry` 自动发布周期（ms） | 0 = 不发布 |
| `statusPeriodMs` | `uint32_t` | `Status` 自动发布周期（ms） | 0 = 不发布 |
| `hardwareVersion` | `const char*` | 硬件版本字符串 | **必填** |
| `firmwareVersion` | `const char*` | 固件版本字符串 | **必填** |

---

## 類 `iDryer::Link`

### 构造函数

```cpp
explicit Link(const Config& cfg);
```

通过 const 引用接收配置。`CFG` 必须在对象整个生命周期内存在（通常是 `static const`）。

### 方法

#### `begin()`

```cpp
bool begin();
```

启动整个 SDK 栈：WiFi/Improv、云端状态机、HTTP claim、MQTT、本地 WebSocket、NVS 持久化。

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

唯一必需的 tick。它维护 WiFi/MQTT/LocalAccess，并按定时器自动发布遥测和状态。

每次 `loop()` 迭代都要调用。不调用它，连接就不会被维护。

```cpp
void loop() {
    link.loop();  // first in loop(), before product logic
}
```

*Source: `iDryer-Storage/src/main.cpp:253`, `iHeater-link/src/main.cpp:381`.*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

立即发布 `link.telemetry` 的当前状态，不受 `telemetryPeriodMs` 定时器影响。

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

立即发布 `link.status` 的当前状态。处理命令后如果新状态必须立刻反映到门户中，请调用它。

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

向 `idryer/{serial}/events` topic 发布事件。会立即发送。

| 参数 | 类型 | 用途 |
|-----------|------|---------|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | 事件代码，例如 `"OVERHEAT"`、`"SESSION_COMPLETE"` |
| `message` | `const char*` | 任意调试文本 |
| `unitId` | `uint8_t` | 单元索引（0..unitsCount-1），或 `0xFF` 表示整个设备 |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

注册业务命令 callback（`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`），这些命令可来自 MQTT 或 Local WS。命令来源对业务代码透明。

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

**重要：** 如果设置了 `runtime()->setCommandHandler(...)`，此 callback 不会被调用；完整 dispatcher 会拦截所有命令。

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

Registers a callback for `commands/profile` — a multi-step drying schedule.

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

集成连接状态变化时调用（HA、Bambu、Moonraker）。这是可选 callback。

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

云端 claim 流程返回需要在门户中输入的 PIN 时调用。

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

设备序列号（来自 NVS 的字符串，在 claim 过程中分配）。claim 完成前为空字符串。

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

仅在 WiFi 凭据尚未设置时写入 NVS。请在 `begin()` 前调用。用于带硬编码凭据的开发环境。

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

始终覆盖 NVS 中的 WiFi 凭据。用于开发辅助和强制重新 provisioning。

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

手动启动云端 claim 流程（provision → register → check-claim）。成功时调用已注册的 `onClaimPin` callback。如果请求被接受则返回 `true`。

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

从 NVS 中移除设备 token 并重启芯片。重启后设备处于未 claim 状态，自动 claim 流程会重新开始。此函数不会返回。

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

通向 integrations manager 的出口，用于产品侧接线（Moonraker 腔体目标温度 callback、Bambu 打印机状态等）。

需要 `#include <integrations/common/link_integrations_manager.h>`。

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

通向 SDK MQTT client 的出口，供需要发布自有 topic 或接入命令路由的组件使用（例如 `MenuBridge`）。

需要 `#include <mqtt/mqtt_client.h>`。

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

通向双发布辅助器的出口，可同时把一个 payload 发送到 MQTT 和 Local WS。用于必须像自动发布遥测一样到达 LAN 客户端的产品响应。

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

通向 SDK runtime 的出口，用于设置完整命令 handler，以替代 facade dispatcher。设置 `setCommandHandler(...)` 后，facade 的 `onRequest`/`onProfile` 不再通过 MQTT 路径调用。

**重要：** 必须严格在 `begin()` 之后调用；`begin()` 会安装自己的 dispatcher，需要被覆盖。

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// Handler signature:
// void handleCommand(const char* cmd, JsonObjectConst data);
```

需要 `#include <runtime/idryer_runtime.h>`。

---

### Telemetry fields {#telemetry-fields}

由产品在 `loop()` 中填充。SDK 按 `telemetryPeriodMs` 定时器读取它们，并发布到 MQTT 和 Local WS。

| 字段 | 类型 | 配置标志 | 用途 |
|-------|------|-------------|---------|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | 空气温度，°C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | 湿度，% |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | 加热器温度，°C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | 加热功率，0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | 风扇状态 |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | 重量，克 |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

第一个（或唯一）单元的 `unitId` = 0。索引必须小于 `Config.unitsCount`。

`Status` 字段结构相同，但表示运行状态：

| 字段 | 类型 | 用途 |
|-------|------|---------|
| `status.mode[unitId]` | `UnitMode` | 当前单元模式 |
| `status.targetTempC[unitId]` | `float` | 目标温度 |
| `status.durationS[unitId]` | `uint32_t` | 请求时长，秒（0 = 无限期） |
| `status.elapsedS[unitId]` | `uint32_t` | 会话开始后经过的时间，秒 |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### 通過運行時的回調註冊

如果需要完全控制传入命令（例如产品自己处理 `get_config`、`set`、非标准 `invoke`）：

```cpp
// Signature — from idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// Registration — strictly after link.begin()
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — command string (`"set"`, `"invoke"`, `"ping"`, `"get_config"`).
`data` — ArduinoJson `JsonObjectConst` with payload.

使用这种方式时，`onRequest()` 和 `onProfile()` 不会从 MQTT 路径调用；产品会直接处理命令。

---

## 列舉

### `iDryer::DeviceType`

| Value | Numeric | Purpose |
|-------|---------|---------|
| `Unknown` | 0 | None / undefined |
| `Dryer` | 1 | Dryer (iDryer LINK) |
| `Heater` | 2 | Heater |
| `StorageLink` | 4 | Storage Link (ESP32-C3 + LED) |
| `IHeaterLink` | 5 | iHeater Link |

### `iDryer::UnitMode`

`Idle`, `Drying`, `Storage`, `Profile`, `Fault`, `Unknown`

### `iDryer::EventKind`

`Info`, `Warning`, `Error`

### `iDryer::RequestKind`

`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`

### `iDryer::IntegrationKind`

`Ha`, `Bambu`, `Moonraker`

### `iDryer::IntegrationState`

`Disabled`, `Idle`, `Connecting`, `Online`, `ConfigMissing`, `Error`

---

## 何時深入

对于大多数任务，facade 已经足够。如果需要在 facade 之下工作，例如直接使用 `idryer::IdryerRuntime`、`idryer::MqttClient`、`idryer::cloud::LinkIntegrationsManager`，请参阅架构章节。
