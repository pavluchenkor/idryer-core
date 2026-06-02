---
title: "idryer-core 是什么，什么时候使用"
description: "idryer-core 的用途：面向 ESP32 iDryer 设备的通用库，处理 MQTT、Wi-Fi、遥测和门户命令。"
---

# idryer-core 是什么，什么时候使用

当设备需要接入 iDryer 生态系统时使用 `idryer-core`：发布遥测、接收命令、完成绑定并通过 MQTT 通信。它特别适合耗材干燥箱、主动加热腔、照明、传感器和 3D 打印机周边模块。

如果您正在为 iDryer 云构建 ESP32 设备，此库处理 WiFi 配置（Improv）、声称协议、MQTT 会话（TLS、重新连接、时间同步）、定期遥测/状态发布和传入命令路由。大约 500 行的样板代码简化为 `link.begin(); link.loop();`。

## 最小示例

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## 库的功能

- WiFi 连接和保活；初始设置的 Web Serial Improv 配置。
- 声称协议：后端中的设备注册，通过 PIN 声称帐户。
- 与 iDryer 代理的 MQTT 会话：TLS、永久会话、自动重新连接、NTP 时间同步。
- 定期发布遥测（`Telemetry`）和状态（`Status`）。
- 将传入命令（`commands/invoke`、`commands/set`、`commands/ping`）路由到产品处理器。
- 本地 WebSocket 服务器：LAN 客户端看到与云相同的流。
- NVS 持久化：WiFi 凭证、设备令牌、重启间隔的菜单配置。

## 库不做什么

- 不管理产品硬件：风扇、加热器、LED 条、传感器。
- 不包含干燥、存储或照明的业务逻辑。
- 不了解产品特定的菜单参数 — 只是传输它们。
- 没有来自产品的数据不发布遥测：在 `loop()` 中自己填充 `link.telemetry.*`。

## 下一步

- [5 分钟快速开始](../02-quickstart/01-five-minutes.md)
- [完整 API：iDryer::Link](../03-public-api/01-link-api-reference.md)
