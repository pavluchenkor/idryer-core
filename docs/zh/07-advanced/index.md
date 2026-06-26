# 内部结构

本节面向已经越过 facade 层的开发者。如果 `iDryer::Link` 已经满足需求，就不需要阅读这里。

这里描述库的内部组件：设备协调器、UART 传输层、平台抽象和 profile 模型。

- [Runtime](01-runtime.md) — `IdryerRuntime`，入口点 `begin()` / `loop()`
- [UART](02-uart.md) — 双 MCU 设备的二进制帧协议
- [Integrations](03-integrations.md) — Home Assistant、Bambu Lab、Moonraker/Klipper
- [Arduino platform](04-platform-arduino.md) — WiFi、NVS、OTA 接口
- [Profiles](05-profiles.md) — `IProfile` 模型和设备行为
