# 步骤 03 — 遥测：发布传感器数据

完成此步骤后，ESP32 将从 SHT31 传感器读取温度和湿度，并每 10 秒将值发布到门户。门户将将其显示为实时图表。

## 您需要什么

**硬件：**

- SHT31 在 I2C 分接模块上（地址 0x44 或 0x45）
- 线缆：SDA、SCL、VCC（3.3 V）、GND

**软件：**

- PlatformIO
- 库 `robtillaart/SHT31 @ ^0.5.0`

## 步骤

**1. 将 SHT31 连接到 ESP32-C3**（Storage Link 使用的默认引脚）：

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3.3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    仅在主板断电时连接传感器。

**2. 将库添加**到 `platformio.ini`：

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... other dependencies
```

**3. 在 `main.cpp` 中包含 Wire 和传感器**。基于 [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp)：

```cpp
#include <Wire.h>
#include "storage/sensors/Sht31ClimateSensor.h"

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. 在 `setup()` 中初始化**：

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // auto-detects address 0x44 or 0x45
```

如果未找到传感器，`begin()` 返回 `false`。设备将继续运行而不需要它。

**5. 在 `loop()` 中调用 `tick()` 并更新遥测字段：**

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airTempC[0]       = r.temperature;
        s_link.telemetry.airHumidityPct[0] = r.humidity;
    }
}
```

库在 `iDryer::Config` 中的 `telemetryPeriodMs` 设置的间隔处自动将所有 `telemetry.*` 字段发布到 MQTT。默认值为 10 000 毫秒。

**6. 在 `iDryer::Config` 中启用功能：**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```

## 验证

打开串行监视器。成功检测到传感器时：

```
[MAIN] SHT31 at 0x44
```

在门户上，导航到设备页面 — 温度和湿度读数每 10 秒更新。

如果未找到传感器，将记录警告，设备将继续运行。检查地址 0x44/0x45 是否未被总线上的其他设备占用。

## 下一步

- [04-leds.md](04-leds.md) — 用 LED 条颜色可视化湿度。
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — 传感器实现。
