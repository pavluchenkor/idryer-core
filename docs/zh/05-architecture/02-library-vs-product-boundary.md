# 边界：库和产品

## 库中的内容

库（`lib/idryer-core/`）包含：

- 整个网络栈：WiFi、HTTP、MQTT、TLS。
- provisioning/claiming 协议。
- 云端状态机（`CloudStateMachine`）。
- UART 桥和帧协议。
- 集成客户端（Bambu、HA、Moonraker）。
- 设备接口（`IWifiManager`、`ICredentialStore`、`IHttpClient`、`IProfile`）。
- 这些接口的 Arduino 实现。
- MQTT topic 和 publish/subscribe 逻辑。

判断代码是否属于库的标准：**任何产品、任何硬件都能不修改地使用它**。

## 产品中的内容

产品（`src/`）包含：

- `IProfile` 实现 — 配置、info payload、`applyConfig`。
- 设备特定的业务逻辑（LED 控制、干燥、加热）。
- `onInvoke` / `onSetCommand` handler。
- 产品传感器和遥测发布。
- 外设初始化（FastLED、Wire、ImprovWiFi）。
- `main.cpp` 中的组合根。

判断代码是否属于产品的标准：**如果不改变硬件或配置，这段代码就没有意义**。

## 具体示例

| 代码 | 放在哪里 | 原因 |
|------|---------------|-----|
| `MqttClient` | 库 | 每个产品都需要 MQTT |
| `CloudStateMachine` | 库 | provisioning/claiming 对所有产品相同 |
| `ArduinoWifiManager` | 库 | WiFi 连接不依赖产品 |
| `LedStripProfile` | 产品 | Storage Link 专用 TODO: 在整个文档中统一 Storage 名称 |
| `LedStripExecutor` | 产品 | 控制 FastLED，其他设备不需要 |
| `Sht31ClimateSensor` | 产品 | 某个具体产品的具体传感器 |
| `StorageTelemetryPublisher` | 产品 | 知道 Storage Link 的遥测格式 |
| `IProfile` | 库 | 库调用的合约 |
| `BambuClient` | 库 | 集成在 iDryer 和 iHeater 间复用 |

## 作为边界的接口

库只通过 `IProfile` 了解产品。所有交互都经过五个方法：

```cpp
profile->onOnline();               // library → product: first time going online
profile->loop();                   // library → product: every cycle
profile->buildInfoJson(buf, len);  // library → product: info payload needed
profile->getConfig(doc);           // library → product: config needed
profile->applyConfig(id, val);     // library → product: set command received
```

产品通过 `MqttClient` 了解库（用于发布遥测/事件），也通过 `ActionDispatcher` 回调了解库（用于命令）。

## 不得越过边界的内容

- 库不得 include 产品头文件。
- 产品不得直接调用 `CloudStateMachine::handleProvisioning()` 或其他私有栈方法，只能通过公共 API。
- 产品遥测通过 `s_mqtt.publishTelemetry()` 直接发布；runtime 不会看到它。
