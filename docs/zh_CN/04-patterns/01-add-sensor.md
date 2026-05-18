# 添加传感器

## 何时使用

如果设备需要定期读取物理传感器（温度、湿度、重量等）并将读数发布到云或 LAN 客户端——使用这个配方。

## 现成代码

复制到您的项目中并将 `MyClimate` 替换为您的类名：

```cpp
// MyClimate.h — 产品传感器驱动程序
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // 非阻塞，无 delay()
    float temperature() const;
    float humidity()    const;
    bool  ok()          const;
};
```

```cpp
// main.cpp
#include <iDryer.h>
#include "MyClimate.h"

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link  s_link(CFG);
static MyClimate     s_climate;
static bool          s_sensorOk = false;

void setup() {
    s_sensorOk = s_climate.begin();
    s_link.begin();
}

void loop() {
    s_link.loop();

    if (s_sensorOk) {
        s_climate.tick(millis());
        if (s_climate.ok()) {
            s_link.telemetry.airTempC[0]       = s_climate.temperature();
            s_link.telemetry.airHumidityPct[0] = s_climate.humidity();
        }
    }
    // 发布是自动的，按 Config 中的 telemetryPeriodMs 计时器。
}
```

## 解释

产品仅在 `loop()` 中填充 `s_link.telemetry.*` 字段。外观每 `Config.telemetryPeriodMs` 毫秒自动将它们发布到 MQTT 和 Local WS——无需手动调用 `publishTelemetryNow()`。这是与手动 MQTT 的关键区别：无 `StaticJsonDocument`、无 `publishTelemetry`、无单独的发布者类。

如果需要立即发布读数在计时器外——调用 `s_link.publishTelemetryNow()`。

`Config` 中的 `hasAirTemp` / `hasAirHumidity` 标志控制哪些字段出现在 JSON 中。标志为 `false` 的字段不被发布。

完整的遥测字段列表：[遥测字段](../03-public-api/01-link-api-reference.md#telemetry-fields)。

## 仓库中的完整示例

参考实现：`Sht31ClimateSensor` + 在 `iDryer-Storage/src/main.cpp` 中填充 `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]`。
