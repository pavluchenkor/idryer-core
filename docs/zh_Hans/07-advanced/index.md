# 内部实现

本部分针对那些已经超越了外观的人。如果 `iDryer::Link` 满足您的需求——您不需要来这里。

它描述了内部库组件：设备协调器、UART 传输层、平台抽象和配置文件模型。

- [运行时](01-runtime.md) — `IdryerRuntime`、入口点 `begin()` / `loop()`
- [UART](02-uart.md) — 双 MCU 设备的二进制帧协议
- [集成](03-integrations.md) — Home Assistant、Bambu Lab、Moonraker/Klipper
- [Arduino 平台](04-platform-arduino.md) — WiFi、NVS、OTA 接口
- [配置文件](05-profiles.md) — `IProfile` 模型和设备行为
