# 步驟 03 — 遙測：發布傳感器數據

完成此步驟後，ESP32 將從 SHT31 傳感器讀取溫度和濕度，並每 10 秒將值發布到門戶。門戶將將其顯示為實時圖表。

## 您需要什麼

**硬件：**

- SHT31 在 I2C 分接模塊上（地址 0x44 或 0x45）
- 線纜：SDA、SCL、VCC（3.3 V）、GND

**軟件：**

- PlatformIO
- 庫 `robtillaart/SHT31 @ ^0.5.0`

## 步驟

**1. 將 SHT31 連接到 ESP32-C3**（Storage Link 使用的默認引腳）：

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3.3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    僅在主機板斷電時連接傳感器。

**2. 將庫添加**到 `platformio.ini`：

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... other dependencies
```

**3. 在 `main.cpp` 中包含 Wire 和傳感器**。基於 [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp)：

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

如果未找到傳感器，`begin()` 返回 `false`。設備將繼續運行而不需要它。

**5. 在 `loop()` 中調用 `tick()` 並更新遙測字段：**

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

庫在 `iDryer::Config` 中的 `telemetryPeriodMs` 設置的間隔處自動將所有 `telemetry.*` 字段發布到 MQTT。默認值為 10 000 毫秒。

**6. 在 `iDryer::Config` 中啟用功能：**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```

## 驗證

打開串行監視器。成功檢測到傳感器時：

```
[MAIN] SHT31 at 0x44
```

在門戶上，導航到設備頁面 — 溫度和濕度讀數每 10 秒更新。

如果未找到傳感器，將記錄警告，設備將繼續運行。檢查地址 0x44/0x45 是否未被總線上的其他設備佔用。

## 下一步

- [04-leds.md](04-leds.md) — 用 LED 條顏色可視化濕度。
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — 傳感器實現。
