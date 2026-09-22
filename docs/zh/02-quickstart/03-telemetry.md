# 遥测：卡片上的传感器

完成本页后，设备从 SHT31 读取温度和湿度，卡片上会显示它们的单元格以及一个自定义数值——露点。

## 你需要

- 一个 SHT31 模块（I2C，地址 0x44 或 0x45）；
- 导线：SDA、SCL、3.3 V、GND；
- 在 `platformio.ini` 中：

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    请在板子断电时连接传感器。

## 代码

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // 温度单元格
    .hasAirHumidity  = true,    // 湿度单元格
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // 地址 0x44 或 0x45，由模块跳线决定

// 按 Magnus 公式计算露点：自定义数值的示例。
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // 无数据即 NAN：字段不发送，卡片显示“—”。
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA、SCL：你的板子的引脚
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // 每 2 秒读取一次
}

void loop() {
    s_link.loop();
}
```

## 工作原理

- `Config` 中的 `hasAirTemp` 和 `hasAirHumidity` 会在卡片上提供温度和湿度单元格。门户上不需要代码。
- `s_link.telemetry.*` 字段由核心自动发布：只要有一个单元在工作就每 30 秒一次，否则每 60 秒一次。周期由 `Config.telemetryPeriodMs` 和 `telemetryPeriodIdleMs` 修改；0 表示契约中的值。
- `NAN` 表示无数据：字段不发布。不要用 0 代替，否则图表上会出现虚假的下跌。
- 自定义数值：`onTelemetryPublish` 在发布前向遥测添加字段，`card().sensor(id, label, unit, path, deviceClass)` 在卡片上声明它。`path` 是遥测中的路径；`deviceClass` 可选，决定图标和格式。
- `s_link.every(ms, fn)` 按指定周期从 `loop()` 调用函数，不会阻塞连接。

## 检查

烧录后一分钟内，卡片显示温度、湿度和露点。没有传感器时单元格为空，设备继续工作。

## 下一步

[LED 灯带](04-leds.md)。
