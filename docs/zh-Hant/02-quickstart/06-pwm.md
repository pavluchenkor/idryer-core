# PWM 加熱：模式、工作階段、功率

完成本頁後，卡片可以用溫度和時間啟動加熱，顯示工作階段和功率，裝置則依據感測器用 PWM 輸出保持溫度。

## 你需要

- [遙測一步](03-telemetry.md) 中的 SHT31 感測器；
- 腳位 3 上一個邏輯電位 MOSFET（3.3 V 即可導通），以及符合其供電電壓的加熱器；
- 在 `platformio.ini` 中：`robtillaart/SHT31 @ ^0.5.0`。

!!! warning
    沒有熱熔斷器的加熱器不要無人看管：韌體可能當機，MOSFET 也可能擊穿短路。

## 程式碼

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // MOSFET 閘極
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // 工作週期 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // 加熱功率儲存格
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "PWM Heater",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);

static void readSensor() {
    const bool ok = s_sht.read();
    s_link.telemetry.airTempC[0]       = ok ? s_sht.getTemperature() : NAN;
    s_link.telemetry.airHumidityPct[0] = ok ? s_sht.getHumidity()    : NAN;
}

static uint32_t s_startMs = 0;

static void stopHeat(uint8_t unit) {
    ledcWrite(PWM_CHANNEL, 0);
    s_link.telemetry.heaterPower01[unit] = 0.0f;
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 表示不限時
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// 每秒一次：依與設定值的差計算功率，更新工作階段時間。
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // 沒有讀數就關閉加熱器：絕不盲目加熱。
    float power = 0.0f;
    if (!isnan(t)) power = constrain((s_link.status.targetTempC[0] - t) * 0.1f, 0.0f, 1.0f);
    ledcWrite(PWM_CHANNEL, (uint32_t)(power * 255));
    s_link.telemetry.heaterPower01[0] = power;

    const uint32_t elapsed = (millis() - s_startMs) / 1000;
    s_link.status.elapsedS[0] = elapsed;
    if (s_link.status.durationS[0] && elapsed >= s_link.status.durationS[0]) stopHeat(0);
}

void setup() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(8, 9);
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");

    s_link.every(2000, readSensor);
    s_link.every(1000, regulate);
}

void loop() {
    s_link.loop();
}
```

## 運作原理

- 模式為 `HEATING` 的操作會啟動工作階段：回呼設定模式、設定值和時長，並呼叫 `publishStatusNow()`。卡片依模式顯示工作階段區塊和 **Stop**。
- 門戶和應用程式會自動為 `target_temperature` 和 `duration` 這兩個 purpose 加上標籤。`args` 中的數值已被限制在參數範圍內。
- `durationS = 0` 表示不限時；卡片把 `elapsedS` 顯示為已用時間。
- `hasHeater` 提供功率儲存格。工作時，核心傳送最近兩個發布週期內 `heaterPower01` 的平均值，所以 PWM 的頻繁變化不會讓卡片上的數值跳動。
- 這裡的調節器是比例式：比設定值低 10 °C 即為滿功率。需要精確保持時，換成 PID。
- `ledcSetup` / `ledcAttachPin` 是 arduino-esp32 2.x 的 API。在 3.x 中使用 `ledcAttach(pin, freq, bits)` 和 `ledcWrite(pin, duty)`。

## 檢查

在卡片上啟動 **Heat**：出現帶有設定值和時間的工作階段區塊，功率儲存格顯示加熱器的開啟比例。**Stop** 關閉輸出。

## 下一步

- [裝置卡片：card 清單](../09-add-product/02-add-widget.md)：來自裝置選單的參數、設定檔、卡片版面。
- [如何新增產品](../09-add-product/01-add-new-product.md)。
