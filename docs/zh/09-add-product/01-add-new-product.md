---
title: "如何添加基于 idryer-core 的新产品"
description: "基于 iDryer::Link 外观类的新 iDryer 设备清单：项目、最小固件、在应用中配置 Wi-Fi 和绑定、遥测、设置菜单、设备卡片、合约。"
---

# 如何添加基于 idryer-core 的新产品

当你在 `idryer-core` 上开发新产品（耗材烘干机、加热模块、照明、传感器或其他模块）时，请使用本指南。它说明核心库替你完成了什么，以及产品代码需要补充什么。

一个可以完整编译的示例是 Build-Your-Own-iDryer 文档中的加热储料柜（`example/09-cabinet`）：它覆盖了本页的全部内容。

---

## 产品的基础

产品通过一个对象与核心库交互——外观类 `iDryer::Link`（`<iDryer.h>`）。在 `s_link.begin()` 和 `s_link.loop()` 中，核心库会：

- 通过无线方式（ESPTouch）从 iDryer 应用、或通过 USB（Improv）从网页安装程序获取 Wi-Fi 网络，并保持连接；
- 把设备绑定到账户：等待一次性绑定令牌——来自应用（经局域网）或来自串口（`PAIR_TOKEN:<token>`）——并在门户上换取永久密钥；
- 连接 MQTT，并按 `Config` 中的周期发布 `telemetry` 和 `status`；
- 在局域网中广播自己（mDNS `_idryer._tcp`），并通过 WebSocket 接收应用的命令；
- 发布设备卡片的 card 清单。

更底层的类（`IdryerRuntime`、`CloudStateMachine`、`LocalAccess` 等）是核心库的内部实现：绑定令牌只有在 `iDryer::Link` 内部才会传到云端部分。请在外观类之上构建产品。

---

## 1. 项目

`idryer-core` 放在 `lib/idryer-core/`（复制或符号链接）；PlatformIO 从它的 `library.json` 获取核心库的依赖。最小的 `platformio.ini`：

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; espMqttClient 依赖中的 ESP8266 传输层：无法在 ESP32 上编译
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

没有 `lib_ignore = ESPAsyncTCP`，构建会在 `ESPAsyncTCP.cpp` 处失败；没有 `MQTT_BROKER` 和 `MQTT_PORT`，核心库无法编译。

---

## 2. 最小固件

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 不是 iDryer 产品：卡片来自清单
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // 设备在门户上被解绑：清除密钥，等待重新绑定。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // 你的传感器函数
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` 启动 Wi-Fi、绑定、MQTT 和本地访问；`s_link.loop()` 必须一直运行，不能使用 `delay()`。当设备从账户解绑时，门户会发来 `revoke` 命令：`handleRevoke()` 清除密钥，设备等待新的绑定令牌。

---

## 3. Wi-Fi 和绑定——不写在代码里

固件中既没有网络密码，也没有账户数据。用户在 iDryer 应用中连接设备：**连接新设备** → **Wi-Fi** 步骤（应用通过 ESPTouch 发送网络）→ **绑定** 步骤（应用通过 mDNS 找到设备，从门户获取一次性令牌并交给设备；设备自己在门户上激活令牌）。在 Wi-Fi 连上之前，串口不会输出：核心库把它留给网页安装程序（Improv）。

分步说明和预期日志——Build-Your-Own-iDryer，“在核心库上启动固件”一章。

---

## 4. 数据：遥测和状态

- `Config` 中的 `has*` 标志决定哪些词汇字段进入遥测、卡片上出现哪些单元格。
- 把值写入 `s_link.telemetry`（`airTempC`、`airHumidityPct`、`heaterTempC`、`heaterPower01`、`fanOn`、`servoOpen`）和 `s_link.status`（`mode`、`targetTempC`、`durationS`、`elapsedS`）；核心库按 `Config` 中的周期发布它们，`s_link.publishStatusNow()` 会立即发送状态。
- 自定义字段——通过 `s_link.onTelemetryPublish()`，显示到卡片上——通过 `s_link.card().sensor()`；见[设备卡片](02-add-widget.md)。

---

## 5. 设置：菜单

设置在 `src/menu/menu.yaml` 中描述；生成器 `menu_gen.py` 由此生成 C++ 代码、NVS 存储和菜单 JSON（[作为协议的菜单](../08-contracts/02-menu-as-protocol.md)）。产品代码：

- 在 `s_link.begin()` 之前加载菜单：`menu.initDefaults()`、`menu.loadFromNVS()`、`menu_sync_state_to_cache()`；
- 用 `menu_buildFullJson()` 和 `s_link.devicePublisher()->publishConfigRaw()` 发布菜单——在上线时以及收到 `get_config` 命令时；
- 用 `menu_apply_by_bind()` 应用 `set` 命令（值、NVS 和缓存一次完成），然后重新发布菜单。

完整代码——Build-Your-Own-iDryer，“来自 YAML 的菜单”一章。

---

## 6. 设备卡片

卡片显示什么、启动哪些操作，用 `s_link.card()` 声明——[设备卡片：card 清单](02-add-widget.md)。

---

## 7. 合约

当你添加新主题或修改负载时：

1. 更新 `contracts/mqtt_contract.yaml`；
2. 运行 `contracts/regen.sh` 并提交生成的文件。

---

## 双芯片设备

对于通过 UART 与独立控制器（例如 RP2040）协作的 ESP32，核心库提供 UART 桥接 `idryer_uart.h`；可运行的参考实现是 `idryer-link` 固件。
