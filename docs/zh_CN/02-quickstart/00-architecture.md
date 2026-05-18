# idryer-core 如何运作

idryer-core 是一个针对 ESP32 的库，处理整个云端堆栈：通过 Improv-Serial 进行 WiFi 配置、用于将设备绑定到 idryer.org 帐户的声称协议、具有自动重新连接的 TLS MQTT 会话、来自门户的命令路由和定期遥测发布。

您只需编写特定于您的设备的内容：读取传感器、驱动外设。其余一切都在库内。

## mqtt_contract.yaml — 唯一真实来源

文件 [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 定义：

- **功能** — 每种设备类型支持的外设（加热器、LED 条、传感器）；
- **遥测字段** — MQTT 数据包中的字段名称和数据类型；
- **UART 协议** — ESP32 和协处理器之间的结构；
- **TypeScript 类型** — 用于门户前端。

代码从此文件自动生成：

| 生成的内容 | 位置 |
|---|---|
| `iDryer::Config`（has* 标志） | `src/_generated/iDryer_api.h` |
| MQTT 主题（C++ 常量） | `contracts/_generated/mqtt_topics.h` |
| TypeScript 类型 | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    不要手动编辑 `src/_generated/` 和 `contracts/_generated/` 中的文件 — 它们在下一次重新生成运行时会被覆盖。

## 如何添加新外设

任何新功能的流程都是相同的 — 按钮、CO2 传感器、RFID 读取器。

**1.** 在 [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) 中的 `capability_vocabulary` 添加条目：

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** 运行重新生成：

```bash
cd contracts
./regen.sh
```

之后，`iDryer::Config` 将具有 `hasCo2` 字段，TypeScript 将具有 `HardwareUnitConfigCapabilities.co2`。

**3.** 在您设备的 `main.cpp` 中设置标志：

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** 刷新设备。门户将从 MQTT `/info` 主题读取 `co2: true` 并自动显示相应的 UI 块 — 不需要门户端更改。

对于合约中尚未包含的外设类型，打开 PR 到 idryer-core 存储库，在 `capability_vocabulary` 中添加条目。合并后 — 运行 `regen.sh`。

## 基于此库构建的两个生产产品

**iDryer Storage Link** — 带有 WS2812B LED 条和 SHT31 温度/湿度传感器的 ESP32-C3。

**iHeater Link** — 带有 RMT 输出到 iHeater 加热器的 ESP32-C3，具有 Bambu Lab、Klipper/Moonraker 和 Home Assistant 的集成。

两个产品都通过 PlatformIO `lib_deps` 包括 idryer-core，并只实现其产品特定的逻辑。

## 下一步

- [01-wifi.md](01-wifi.md) — 使用 Improv-Serial 将 ESP32 连接到 WiFi。
- [../../../README.md](../../../README.md) — 库概述和代码生成参考。
