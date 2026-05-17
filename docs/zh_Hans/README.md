# idryer-core — 库文档

`idryer-core` — 基于 ESP32 的 iDryer 设备的 C++ 库(Arduino/PlatformIO)。管理 WiFi、MQTT、云状态机和命令路由。该产品仅实现特定设备的行为。

这是**库**的文档，不是任何特定产品的文档。

产品文档位于 [`docs/ru/`](../../docs/ru/)。

---

## 快速开始

**三件你需要实现的事：**

1. 实现 `IProfile` — 五个方法(config、info、loop)。
2. 组装 `main.cpp` — 静态对象，通过构造函数传递依赖。
3. 注册 `handleCommand` — MQTT 和本地 WS 的单个处理器。

**三件库为你做的事：**

1. 管理 WiFi → 配置 → MQTT 会话。
2. 将传入命令路由到你的 `handleCommand`(除了 `ping`，它由内部处理)。
3. 在正确的时刻调用你的 `IProfile` 方法。

**你可以保持不变的东西：**

- `ArduinoWifiManager`、`ArduinoCredentialStore` 和其他 `Arduino*` 类 — 按原样使用，无需子类化。
- `CloudStateMachine` — 创建它并传递给 `IdryerRuntime`；它从那里自行管理。
- `ActionDispatcher` — invoke/set 的兼容性回退；对于新产品，命令处理通过 `setCommandHandler()`，而不是通过 `ActionDispatcher`。

实际指南：[09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

工作示例：[`examples/`](../../examples/)

---

## 部分

| 部分 | 描述 |
|------|------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | 库的目的、它不做什么、谁使用它 |
| [01-overview/02-module-map](01-overview/02-module-map.md) | 所有模块的表：目的、可选性 |
| [02-getting-started](02-quickstart/01-five-minutes.md) | 新开发人员的简短入门：要连接什么、要刷什么、要期望什么 |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | 产品如何组装堆栈：对象创建顺序、main.cpp 模式 |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | 什么在库中，什么在产品中 |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | 运行中的设备中的数据流：传入命令、传出消息、连接 |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | `MqttClient` 类：构造函数、连接、发布 |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | 所有 MQTT 主题：字符串、负载、保留、QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`：它协调什么、它处理哪些命令 |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | 双 MCU 设备的 UART 网桥 |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu、Home Assistant、Moonraker：设置、限制 |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | 设备接口的 Arduino 实现 |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | `IProfile` 接口、回调、`LedStripProfile` 示例 |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`：目的和修改规则 |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | 在 `idryer-core` 基础上构建新产品的检查清单 |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | 常见问题：WiFi、配置、MQTT、命令、LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | 如何添加传感器(数据源)并发布其读数 |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | 如何添加外设并接收命令 |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | 如何添加平行传输(BLE、HTTP、自定义) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | 在传感器/外设/个人资料/发布者之间传递数据的应用方案 |
