# 遙測：卡片上的感測器

完成本頁後，裝置從 SHT31 讀取溫度和濕度，卡片上會顯示它們的儲存格以及一個自訂數值——露點。

## 你需要

- 一個 SHT31 模組（I2C，位址 0x44 或 0x45）；
- 導線：SDA、SCL、3.3 V、GND；
- 在 `platformio.ini` 中：

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    請在板子斷電時連接感測器。

## 程式碼

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // 溫度儲存格
    .hasAirHumidity  = true,    // 濕度儲存格
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // 位址 0x44 或 0x45，由模組跳線決定

// 依 Magnus 公式計算露點：自訂數值的範例。
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // 無資料即 NAN：欄位不傳送，卡片顯示「—」。
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA、SCL：你的板子的腳位
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // 每 2 秒讀取一次
}

void loop() {
    s_link.loop();
}
```

## 運作原理

- `Config` 中的 `hasAirTemp` 和 `hasAirHumidity` 會在卡片上提供溫度和濕度儲存格。門戶上不需要程式碼。
- `s_link.telemetry.*` 欄位由核心自動發布：只要有一個單元在工作就每 30 秒一次，否則每 60 秒一次。週期由 `Config.telemetryPeriodMs` 和 `telemetryPeriodIdleMs` 修改；0 表示契約中的值。
- `NAN` 表示無資料：欄位不發布。不要用 0 代替，否則圖表上會出現虛假的下跌。
- 自訂數值：`onTelemetryPublish` 在發布前向遙測加入欄位，`card().sensor(id, label, unit, path, deviceClass)` 在卡片上宣告它。`path` 是遙測中的路徑；`deviceClass` 可選，決定圖示和格式。
- `s_link.every(ms, fn)` 依指定週期從 `loop()` 呼叫函式，不會阻塞連線。

## 檢查

燒錄後一分鐘內，卡片顯示溫度、濕度和露點。沒有感測器時儲存格為空，裝置繼續運作。

## 下一步

[LED 燈條](04-leds.md)。
