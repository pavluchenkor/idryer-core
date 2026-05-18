# 打印机集成

集成模块允许 iDryer/iHeater 设备连接到第三方系统：Home Assistant、Bambu Lab（LAN）、Moonraker/Klipper。单独包含：

```cpp
#include <idryer_integrations.h>
```

**集成是可选模块。** 存储链接不使用它们。它们为 iDryer LINK 和 iHeater LINK 实现。

## LinkIntegrationsManager

模块的主要类。一次管理一个活跃集成。通过产品的 `CommandHandler` 连接——与用于 MQTT 和本地 WS 的相同处理程序。

```cpp
LinkIntegrationsStore intStore;
idryer::cloud::LinkIntegrationsManager intManager(&s_mqtt, &intStore);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "link_integration") == 0) {
        intManager.handleLinkIntegrationCommand(data); return;
    }
    if (strcmp(cmd, "bambu_apply") == 0) {
        intManager.handleBambuApplyCommand(data); return;
    }
    // ... 其他产品命令 ...
}

// 在 setup() 中：
runtime.setCommandHandler(handleCommand);
local.setCommandSink(handleCommand);
intManager.begin(); // 在 runtime.begin() 之后
// 在 loop() 中：intManager.loop();
```

管理器通过 `LinkIntegrationsStore` 将所有三个集成的配置存储在 NVS 中。切换活跃集成通过命令完成：

```json
{"active": "bambu"}     // 或 "ha"、"moonraker"、"none"
```

状态在改变时和每 30 秒发布到 `idryer/{serial}/integrations/status`（保留）。

## Bambu Lab

`BambuClient` 通过本地网络上的 MQTT 连接到打印机（TLS、端口 8883、自签名证书、`setInsecure`）。

根据设备类型的两种操作模式：

| 模式 | DeviceType | 行为 |
|------|-----------|------|
| **Writer** | Dryer | 在 `bambu_apply` 时向打印机发送 `ams_filament_setting` |
| **Reader** | Heater / IHeaterLink | 订阅 `device/{printerSerial}/report`、将打印机状态传递给回调 |

连接参数：

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

使用从 1 秒到 60 秒的指数退避重新连接。

回调：

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState、s.nozzleTemp、s.trayType、...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — 连接到 HA MQTT 代理（不是 HA 云，而是内置 HA MQTT 服务器）。

通过 `link_integration` 命令配置：

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

适配器支持 mDNS 主机发现（字符串 `homeassistant.local`）和直接 IP 连接。带退避重新连接。

`HaMqttClient` 通过 `intManager.haMqttClient()` 暴露——产品可以通过它发布 HA 实体。

设备必须设置其客户端 ID：

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` 通过 WebSocket（`ws://host:port/websocket`）连接，并使用 JSON-RPC 2.0 订阅 Klipper 对象。

主要用例 — iHeater：通过 `gcode_macro VIRTUAL_CHAMBER` 接收室目标温度。

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

客户端订阅 Klipper 对象，包括 `gcode_macro VIRTUAL_CHAMBER`、`print_stats`、`display_status` 和温度传感器。

回调：

```cpp
intManager.setVirtualChamberCallback([](const VirtualChamberData& vc) {
    // vc.target — 室目标温度
    // vc.available — VIRTUAL_CHAMBER 对象在 Klipper 中可见
});

intManager.setMoonrakerStatusCallback([](const MoonrakerStatus& s) {
    // s.printerState、s.nozzleTemp、s.progress、...
});
```

## 限制

- 一次一个活跃集成。切换是原子的：旧的停止、新的启动。
- 每个设备一个 `BambuClient` 实例（通过静态指针的单例）。
- `LinkIntegrationsStore` 将配置存储在 NVS 中——设置在重启时持久化。
- 设备必须指定其类型（`setDeviceType`）以进行正确的 Bambu 模式选择：
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // 或 Heater、IHeaterLink
  ```
