# LED 燈條：選擇效果和顏色開燈

完成本頁後，卡片上會有可選擇效果和顏色的 **Turn on** 操作，以及 **Turn off**。

## 你需要

- 一條 WS2812B 燈條（或 WS2811、SK6812）；
- 資料線上一個 330–470 Ω 電阻；
- 一個 5 V 電源：每顆 LED 在白色全亮時最多 60 mA，60 顆最多 3.6 A；
- 在 `platformio.ini` 中：

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 加上核心放不進 ESP32-C3 的快閃記憶體（1.25 MB 分割區需要 1.7 MB）；用 3.10.3 則為 1.16 MB。

!!! warning
    燈條請用獨立的 5 V 電源供電，並把電源和板子的接地相連。板子腳位供電只適合用幾顆 LED 做測試。

## 程式碼

```cpp
#include <Arduino.h>
#include <FastLED.h>
#include <iDryer.h>

#define LED_DATA_PIN 4
#define LED_COUNT    60

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "LED Strip",
};
static iDryer::Link s_link(CFG);

static CRGB s_leds[LED_COUNT];
static const char* const kEffects[] = { "solid", "breathe" };
static CRGB s_color   = CRGB::White;
static bool s_breathe = false;

static void onLightOn(uint8_t unit, JsonObjectConst args) {
    const char* hex = args["color"] | "#FFFFFF";          // "#RRGGBB"
    s_color   = CRGB(strtoul(hex + 1, nullptr, 16));
    s_breathe = strcmp(args["effect"] | "solid", "breathe") == 0;
    s_link.status.mode[unit] = iDryer::UnitMode::LightAnimation;
    s_link.publishStatusNow();
}

static void onLightOff(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit] = iDryer::UnitMode::Idle;
    s_link.publishStatusNow();
}

// 每 20 ms 一幀：顏色，breathe 時亮度平滑變化。
static void drawLeds() {
    const bool on = s_link.status.mode[0] == iDryer::UnitMode::LightAnimation;
    CRGB c = s_color;
    c.nscale8(!on ? 0 : s_breathe ? beatsin8(15, 30, 255) : 255);
    fill_solid(s_leds, LED_COUNT, c);
    FastLED.show();
}

void setup() {
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(s_leds, LED_COUNT);
    FastLED.setBrightness(128);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("light_on", "LIGHT_ANIMATION", onLightOn)
        .name("ru", "Включить").name("en", "Turn on")
        .select("effect", "effect", kEffects, 2, "solid")
        .color("color", "rgb_color", "#FFFFFF");
    card.action("light_off", "IDLE", onLightOff)
        .name("ru", "Выключить").name("en", "Turn off");

    s_link.every(20, drawLeds);
}

void loop() {
    s_link.loop();
}
```

## 運作原理

- `card().action(id, mode, cb)` 宣告一個操作。`mode` 是執行後單元的模式：開燈為 `LIGHT_ANIMATION`，關燈為 `IDLE`。
- `.name("ru", …)` 和 `.name("en", …)` 是按鈕名稱；其他語言下卡片顯示英文名稱。
- `.select(...)` 和 `.color(...)` 是開燈的參數。門戶和應用程式會自動為 `effect` 和 `rgb_color` 這兩個 purpose 加上標籤；不在清單中的值由核心替換為預設值。
- 回呼依 id 在 `args` 中取得參數，設定 `status.mode` 並呼叫 `publishStatusNow()`。
- 卡片依單元的模式選擇顯示：關燈時顯示 **Turn on** 表單；開燈時顯示點亮的區塊和 **Turn off** 按鈕。

## 檢查

在卡片上選擇效果和顏色並點選 **Turn on**：燈條亮起，卡片顯示 **Turn off**。

## 下一步

[無模式的操作](05-actions.md)。
