# LED 灯带：选择效果和颜色开灯

完成本页后，卡片上会有可选择效果和颜色的 **Turn on** 操作，以及 **Turn off**。

## 你需要

- 一条 WS2812B 灯带（或 WS2811、SK6812）；
- 数据线上一个 330–470 Ω 电阻；
- 一个 5 V 电源：每颗 LED 在白色全亮时最多 60 mA，60 颗最多 3.6 A；
- 在 `platformio.ini` 中：

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 加上核心放不进 ESP32-C3 的闪存（1.25 MB 分区需要 1.7 MB）；用 3.10.3 则为 1.16 MB。

!!! warning
    灯带请用单独的 5 V 电源供电，并把电源和板子的地线相连。板子引脚供电只适合用几颗 LED 做测试。

## 代码

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

// 每 20 ms 一帧：颜色，breathe 时亮度平滑变化。
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

## 工作原理

- `card().action(id, mode, cb)` 声明一个操作。`mode` 是执行后单元的模式：开灯为 `LIGHT_ANIMATION`，关灯为 `IDLE`。
- `.name("ru", …)` 和 `.name("en", …)` 是按钮名称；其他语言下卡片显示英文名称。
- `.select(...)` 和 `.color(...)` 是开灯的参数。门户和应用会自动为 `effect` 和 `rgb_color` 这两个 purpose 加标签；不在列表中的值由核心替换为默认值。
- 回调按 id 在 `args` 中取得参数，设置 `status.mode` 并调用 `publishStatusNow()`。
- 卡片按单元的模式选择显示：关灯时显示 **Turn on** 表单；开灯时显示点亮的区块和 **Turn off** 按钮。

## 检查

在卡片上选择效果和颜色并点击 **Turn on**：灯带亮起，卡片显示 **Turn off**。

## 下一步

[无模式的操作](05-actions.md)。
