# 無模式的操作：繼電器和呼叫裝置

完成本頁後，卡片上會有以分鐘為單位的 **Ventilate** 表單，標題列中有呼叫裝置的按鈕。

## 你需要

- 腳位 5 上一個可由 3.3 V 控制的繼電器模組（或電晶體開關）；
- 接在繼電器上的風扇或其他負載。

!!! warning
    230 V 市電負載必須有絕緣、保險絲和外殼。沒有市電經驗時，請用低壓負載搭建。

## 程式碼

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // 「風扇開/關」儲存格
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 表示繼電器關閉
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // 開/關的變化會立即發布
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // 每 250 ms
    if (s_ventUntilMs && (int32_t)(millis() - s_ventUntilMs) >= 0) {
        s_ventUntilMs = 0;
        digitalWrite(RELAY_PIN, LOW);
        s_link.telemetry.fanOn[0] = false;
    }
    if (s_blinks) digitalWrite(LED_PIN, --s_blinks % 2);
}

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // 無模式：單元不會變為「占用」，表單留在卡片上。
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // 卡片標題列中的呼叫裝置按鈕。
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## 運作原理

- 無模式的操作（`mode` = `nullptr`）不會讓單元變為「占用」：卡片不顯示工作階段區塊，表單留在原處。
- purpose `duration` 是以分鐘為單位的時間：門戶和應用程式會自動為該欄位加上時間標籤，範圍來自 `.param(...)`。
- `hasFan` 提供「風扇開/關」儲存格。`telemetry.fanOn` 的變化由核心立即發布，不等週期。
- `.deviceClass("identify")` 是卡片標題列中常駐的呼叫裝置按鈕；`clear_errors` 用於清除錯誤。

## 檢查

在卡片上設定時間並點選 **Ventilate**：繼電器吸合，風扇儲存格顯示「開」。標題列的呼叫按鈕會讓板載 LED 閃爍。

## 下一步

[PWM 加熱](06-pwm.md)。
