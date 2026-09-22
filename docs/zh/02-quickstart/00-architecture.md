# idryer-core 的工作方式

idryer-core 是一个 ESP32 库。它负责把设备连接到门户和应用的全部工作：

- Wi-Fi：网络由 iDryer 应用（ESPTouch）或通过 USB 的网页（Improv）传给设备；
- 用一次性令牌绑定到账户，以及解绑；
- 带重连的安全 MQTT 会话；
- 局域网访问：即使没有互联网，应用也能控制设备；
- 发布遥测和状态，传递命令；
- 无线固件更新；
- 门户和应用中的设备卡片。

你只写自己的部分：读取传感器、驱动负载、声明卡片显示什么以及它启动哪些操作。

## 唯一的入口：`iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 你自己的设备
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // 温度单元格
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // 你的代码
}
```

| 你 | 核心 |
|---|---|
| 填写 `s_link.telemetry.*` | 每 30 秒发布一次，空闲时每 60 秒一次 |
| 修改 `s_link.status.*`（模式、设定值、时间）并调用 `publishStatusNow()` | 送达状态；卡片按模式切换 |
| 声明 `s_link.card()`：传感器、控件、操作 | 生成 card 清单并发布 |
| 注册 `s_link.onCommand(...)` | 转交来自云端和局域网的命令 |

## 设备卡片

门户和应用根据设备发送的 card 清单绘制卡片：

- `Config.has*` 标志提供现成的单元格：温度、湿度、加热功率、风扇等；
- `card().sensor(...)` 按遥测中的路径添加自定义数值；
- `card().action(...)` 声明一个操作：执行后单元的模式和启动参数。

门户上不需要为此写代码。详见 [设备卡片：card 清单](../09-add-product/02-add-widget.md)。

## `mqtt_contract.yaml` 是唯一的事实来源

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 描述协议：主题、遥测和状态字段、设备能力、card 清单。由它生成：

| 内容 | 位置 |
|---|---|
| `iDryer::Config`（`has*` 标志）和 API 结构 | `src/_generated/iDryer_api.h` |
| MQTT 主题 | `contracts/_generated/mqtt_topics.h` |
| ESP32 ↔ 控制器的 UART 协议 | `contracts/_generated/uart_protocol.h` |
| 门户用的 TypeScript 类型 | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    不要手动修改 `_generated/` 中的文件：`contracts/regen.sh` 会根据契约覆盖它们。

自定义数值不需要修改契约：用 `card().sensor(...)` 声明即可。当所有产品都需要一项新能力时才修改契约：先改 `mqtt_contract.yaml`，再运行 `regen.sh`，最后改代码。

## 基于核心的产品

- **iDryer Link**：干燥机的通信模块，一块 ESP32 挨着控制器，通过 UART 通信。
- **iDryer Storage**：线盘架照明，可寻址灯带和 SHT31 传感器。
- **iHeater Link**：控制 iHeater 加热器，集成 Bambu Lab、Klipper/Moonraker 和 Home Assistant。

## 下一步

[5 分钟运行](01-five-minutes.md)。
