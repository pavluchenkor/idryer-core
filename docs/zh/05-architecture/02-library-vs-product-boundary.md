# 边界：库和产品

## 库中存在什么

库（`lib/idryer-core/`）包含：

- 整个网络堆栈：WiFi、HTTP、MQTT、TLS。
- 配置/声称协议。
- 云状态机（`CloudStateMachine`）。
- UART 桥接和帧协议。
- 集成客户端（Bambu、HA、Moonraker）。
- 设备接口（`IWifiManager`、`ICredentialStore`、`IHttpClient`、`IProfile`）。
- 这些接口的 Arduino 实现。
- MQTT 主题和发布/订阅逻辑。

属于库的代码的测试：**任何产品和任何硬件都可以使用它而不需修改**。

## 产品中存在什么

产品（`src/`）包含：

- `IProfile` 实现——配置、信息有效负载、`applyConfig`。
- 特定于设备的业务逻辑（LED 控制、干燥、加热）。
- `onInvoke` / `onSetCommand` 处理程序。
- 产品传感器和遥测发布。
- 外围设备初始化（FastLED、Wire、ImprovWiFi）。
- `main.cpp` 中的组合根。

属于产品的代码的测试：**在不改变硬件或配置的情况下，它是没有意义的**。

## 具体例子

| 代码 | 位置 | 原因 |
|------|------|------|
| `MqttClient` | 库 | 每个产品都需要 MQTT |
| `CloudStateMachine` | 库 | 配置/声称对所有产品相同 |
| `ArduinoWifiManager` | 库 | WiFi 连接不取决于产品 |
| `LedStripProfile` | 产品 | 特定于存储链接 |
| `LedStripExecutor` | 产品 | 控制 FastLED，其他设备不需要 |
| `Sht31ClimateSensor` | 产品 | 特定产品的特定传感器 |
| `StorageTelemetryPublisher` | 产品 | 知道存储链接遥测格式 |
| `IProfile` | 库 | 库调用的契约 |
| `BambuClient` | 库 | 集成在 iDryer 和 iHeater 中重复使用 |

## 接口作为边界

库仅通过 `IProfile` 知道产品。所有交互都通过五个方法进行：

```cpp
profile->onOnline();               // 库 → 产品：第一次在线
profile->loop();                   // 库 → 产品：每个周期
profile->buildInfoJson(buf, len);  // 库 → 产品：需要信息有效负载
profile->getConfig(doc);           // 库 → 产品：需要配置
profile->applyConfig(id, val);     // 库 → 产品：收到 set 命令
```

产品通过 `MqttClient`（用于发布遥测/事件）和通过 `ActionDispatcher` 回调（用于命令）了解库。

## 不能跨越边界的内容

- 库不能包含产品头文件。
- 产品不能直接调用 `CloudStateMachine::handleProvisioning()` 或其他私有堆栈方法——仅通过公开 API。
- 产品遥测直接通过 `s_mqtt.publishTelemetry()` 发布——运行时不会看到它。
