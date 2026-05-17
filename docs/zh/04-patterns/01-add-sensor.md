# 新增感測器

## 何時使用

如果裝置需要定期讀取物理感測器（溫度、濕度、重量等），並將讀數發佈到雲端或 LAN 客戶端，請使用此配方。

## 現成可用的代碼

複製到您的專案中，並將 `MyClimate` 替換為您的類別名稱：

```cpp
// MyClimate.h — product sensor driver
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // non-blocking, no delay()
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
    // Publishing is automatic, on the telemetryPeriodMs timer from Config.
}
```

## 說明

產品只在 `loop()` 中填充 `s_link.telemetry.*` 欄位。門面在 `Config.telemetryPeriodMs` 毫秒後自動將它們發佈到 MQTT 和 Local WS。這與手動 MQTT 不同，無需呼叫 `publishTelemetryNow()` 手動呼叫。這是與手動 MQTT 的關鍵區別：沒有 `StaticJsonDocument`、沒有 `publishTelemetry`、沒有單獨的發佈者類別。

如果需要在計時器外立即發佈讀數，請呼叫 `s_link.publishTelemetryNow()`。

`Config` 中的 `hasAirTemp` / `hasAirHumidity` 標誌控制 JSON 中出現哪些欄位。標誌為 `false` 的欄位不會發佈。

遙測欄位的完整清單：[遙測欄位](../03-public-api/01-link-api-reference.md#telemetry-fields)。

## 儲存庫中的完整示例

參考實現：`Sht31ClimateSensor` + 在 `iDryer-Storage/src/main.cpp` 中填充 `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]`。
