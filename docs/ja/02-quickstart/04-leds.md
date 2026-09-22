# LED テープ：エフェクトと色を選んで点灯

このページを終えると、カードにエフェクトと色を選べる **Turn on** アクションと **Turn off** が表示されます。

## 必要なもの

- WS2812B テープ（または WS2811、SK6812）；
- データ線に 330–470 Ω の抵抗；
- 5 V 電源：白の最大輝度で LED 1 個あたり最大 60 mA、60 個で最大 3.6 A；
- `platformio.ini` に：

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 はコアと合わせると ESP32-C3 のフラッシュに収まりません（1.25 MB のパーティションに対して 1.7 MB）。3.10.3 なら 1.16 MB です。

!!! warning
    テープは別の 5 V 電源から給電し、電源とボードの GND をつないでください。ボードのピンからの給電は、数個の LED での試験だけにしてください。

## コード

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

// 20 ms ごとのテープのフレーム：色、breathe では明るさを滑らかに変える。
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

## 仕組み

- `card().action(id, mode, cb)` はアクションを宣言します。`mode` は実行後のユニットのモードで、点灯は `LIGHT_ANIMATION`、消灯は `IDLE` です。
- `.name("ru", …)` と `.name("en", …)` はボタン名です。その他の言語ではカードに英語名が表示されます。
- `.select(...)` と `.color(...)` は点灯のパラメーターです。purpose の `effect` と `rgb_color` はポータルとアプリが自動でラベル付けします。リストにない値はコアが既定値に置き換えます。
- コールバックは `args` で id ごとにパラメーターを受け取り、`status.mode` を設定して `publishStatusNow()` を呼びます。
- カードはユニットのモードで表示を選びます：消灯中は **Turn on** のフォーム、点灯中は点灯中のブロックと **Turn off** ボタン。

## 確認

カードでエフェクトと色を選び **Turn on** をタップすると、テープが点灯し、カードに **Turn off** が表示されます。

## 次へ

[モードなしのアクション](05-actions.md)。
