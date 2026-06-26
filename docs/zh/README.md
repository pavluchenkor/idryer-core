---
title: "面向 iDryer 设备和自制模块的 idryer-core"
description: "idryer-core 文档：连接 ESP32 设备、MQTT、Wi-Fi、云端状态机，以及耗材干燥箱或 3D 打印机模块的命令。"
---

# 面向 iDryer 设备和自制模块的 idryer-core

当自制耗材干燥箱、加热腔、照明系统或其他 3D 打印机模块需要成为受管理的 iDryer 设备时，可以使用 `idryer-core`。库负责 Wi-Fi、MQTT、命令、遥测和门户通信；产品代码只描述具体设备的行为。

`idryer-core` 是面向基于 ESP32 的 iDryer 设备的 C++ 库（Arduino/PlatformIO）。它管理 WiFi、MQTT、云端状态机和命令路由。产品只实现设备特定的行为。

这是**库**的文档，不是某个具体产品的文档。
产品文档位于 [`docs/ru/`](../../docs/ru/)。

---

## 快速开始

**你需要实现三件事：**

1. 实现 `IProfile` — 五个方法（配置、信息、loop）。
2. 组装 `main.cpp` — 静态对象，通过构造函数传递依赖。
3. 注册 `handleCommand` — MQTT 以及可选本地 WS 的单一处理器。

**库会做三件事：**

1. 管理 WiFi → provisioning → MQTT 会话。
2. 将传入命令路由到你的 `handleCommand`（`ping` 由内部处理）。
3. 在正确时机调用你的 `IProfile` 方法。

**可以保持不变的部分：**

- `ArduinoWifiManager`、`ArduinoCredentialStore` 和其他 `Arduino*` 类 — 直接使用，不需要继承。
- `CloudStateMachine` — 创建后传给 `IdryerRuntime`；之后它会自行管理。
- `ActionDispatcher` — invoke/set 的兼容 fallback；新产品应通过 `setCommandHandler()` 处理命令，而不是通过 `ActionDispatcher`。

实用指南：[09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

可运行示例：[`examples/`](../../examples/)

---

## 章节

| 章节 | 说明 |
|---------|-------------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | 库的用途、不做什么、谁会使用它 |
| [01-overview/02-module-map](01-overview/02-module-map.md) | 所有模块表：用途和是否可选 |
| [02-getting-started](02-quickstart/01-five-minutes.md) | 给新开发者的短入口：需要连接什么、刷写什么、预期看到什么 |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | 产品如何组装栈：对象创建顺序、`main.cpp` 模式 |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | 哪些属于库，哪些属于产品 |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | 运行中设备的数据流：传入命令、传出消息、连接关系 |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | `MqttClient` 类：构造、连接、发布 |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | 所有 MQTT topic：字符串、payload、retained、QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`：协调什么、处理哪些命令 |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | 双 MCU 设备的 UART 桥 |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu、Home Assistant、Moonraker：设置和限制 |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | 设备接口的 Arduino 实现 |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | `IProfile` 接口、回调、`LedStripProfile` 示例 |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`：用途和修改规则 |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | 基于 `idryer-core` 构建新产品的 checklist |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | 常见问题：WiFi、provisioning、MQTT、命令、LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | 如何添加传感器并发布读数 |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | 如何添加外设并接收命令 |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | 如何添加并行传输（BLE、HTTP、自定义） |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | 在传感器、外设、profile 和 publisher 之间传递数据的实用方案 |
