# 打印机集成

集成模块允许 iDryer/iHeater 设备连接到第三方系统：Home Assistant、Bambu Lab（LAN）、Moonraker/Klipper。需要单独 include：

```cpp
#include <idryer_integrations.h>
```

**集成是可选模块。** Storage Link 不使用它们。它们为 iDryer LINK 和 iHeater LINK 实现。

## LinkIntegrationsManager

模块主类。一次管理一个活动集成。通过产品的 `CommandHandler` 接入，也就是 MQTT 和本地 WS 使用的同一个 handler。

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
    // ... other product commands ...
}

// in setup():
runtime.setCommandHandler(handleCommand);
local.setCommandSink(handleCommand);
intManager.begin(); // after runtime.begin()
// in loop(): intManager.loop();
```

manager 通过 `LinkIntegrationsStore` 将三个集成的配置都存入 NVS。使用以下命令切换活动集成：

```json
{"active": "bambu"}     // or "ha", "moonraker", "none"
```

状态会在变化时以及每 30 秒发布到 `idryer/{serial}/integrations/status`（retained）。

## Bambu Lab

`BambuClient` 通过本地网络上的 MQTT 连接到打印机（TLS，端口 8883，自签名证书，`setInsecure`）。

根据设备类型有两种工作模式：

| 模式 | DeviceType | 行为 |
|------|-----------|-----------|
| **Writer** | Dryer | 在 `bambu_apply` 时向打印机发送 `ams_filament_setting` |
| **Reader** | Heater / IHeaterLink | 订阅 `device/{printerSerial}/report`，把打印机状态传给 callback |

连接参数：

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

使用指数退避重连，从 1 秒到 60 秒。

Callback：

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — 连接到 HA MQTT broker（不是 HA cloud，而是内置的 HA MQTT 服务器）。

通过 `link_integration` 命令配置：

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

adapter 支持 mDNS 主机发现（字符串 `homeassistant.local`）和直接 IP 连接。断线后带退避重连。

产品可通过 `intManager.haMqttClient()` 获取 `HaMqttClient`，并用它发布 HA entity。

设备必须设置自己的 client ID：

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` 通过 WebSocket（`ws://host:port/websocket`）连接，并使用 JSON-RPC 2.0 订阅 Klipper 对象。

主要用例是 iHeater：通过 `gcode_macro VIRTUAL_CHAMBER` 获取腔体目标温度。

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

客户端订阅 Klipper 对象，包括 `gcode_macro VIRTUAL_CHAMBER`、`print_stats`、`display_status` 和温度传感器。

Callback：

```cpp
intManager.setVirtualChamberCallback([](const VirtualChamberData& vc) {
    // vc.target — chamber target temperature
    // vc.available — VIRTUAL_CHAMBER object visible in Klipper
});

intManager.setMoonrakerStatusCallback([](const MoonrakerStatus& s) {
    // s.printerState, s.nozzleTemp, s.progress, ...
});
```

## 限制

- 一次只有一个活动集成。切换是原子的：旧集成停止，新集成启动。
- 每个设备一个 `BambuClient` 实例（通过静态指针实现 singleton）。
- `LinkIntegrationsStore` 将配置存入 NVS，设置会跨重启保留。
- 设备必须指定类型（`setDeviceType`），以便选择正确的 Bambu 模式：
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // or Heater, IHeaterLink
  ```
