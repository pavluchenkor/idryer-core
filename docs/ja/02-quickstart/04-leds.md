# ステップ 04 — 表示: センサー データで駆動される LED ストリップ

このステップの後、WS2812B ストリップは湿度に基づいて色が変わり、明るさは `set` コマンド経由でポータルから制御可能になります。

## 必要なもの

**ハードウェア:**

- WS2812B LED ストリップ (または WS2811/SK6812)
- データ ラインの 330–470 Ω 抵抗
- 5 V 電源 (電流はストリップの長さに依存します; 300 LED は最大 18 A を消費します)

**ソフトウェア:**

- ライブラリ `fastled/FastLED @ ^3.6.0`

!!! warning
    ストリップに専用の 5 V 電源を供給します。ボードの 3.3 V または 5 V ピン経由での給電は、少数の LED での簡単なスモーク テストの場合のみ許容されます。

## 手順

**1. FastLED を `platformio.ini` に追加します:**

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... other dependencies
```

**2. `main.cpp` でバッファと実行器を宣言します。** [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp) に基づいています:

```cpp
#include <FastLED.h>
#include "storage/led_strip/led_strip_executor.h"

#define STORAGE_LED_PIN  4
#define STORAGE_MAX_LEDS 300

static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. `setup()` でストリップを初期化します:**

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

`60` をストリップの実際の LED 数に置き換えます。

**4. `loop()` で湿度による色を変更します。** 色スケール: 青 (乾燥) → 黄色 → 赤 (湿気):

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airHumidityPct[0] = r.humidity;

        // Humidity 20%–80% → hue from 160 (blue) to 0 (red).
        float h = constrain(r.humidity, 20.0f, 80.0f);
        uint8_t hue = (uint8_t)(160.0f - (h - 20.0f) / 60.0f * 160.0f);
        fill_solid(s_leds, s_executor.ledsCount(), CHSV(hue, 255, 200));
        FastLED.show();
    }
}
```

**5. ポータルから明るさを制御します。** `setup()` で `set` コマンド ハンドラーを登録します:

```cpp
s_link.onCommand("set", [](JsonObjectConst data) {
    int id  = data["id"]  | -1;
    int val = data["val"] | -1;
    if (id == MENU_BRIGHTNESS && val >= 0 && val <= 255) {
        FastLED.setBrightness((uint8_t)val);
        FastLED.show();
    }
});
```

`MENU_BRIGHTNESS` は [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h) の定数で、`regen.sh` 経由で `menu.yaml` から生成されます。独自のプロダクトでは、名前と値が異なります — プロジェクトの `menu_ids.h` を確認してください。

## 検証

フラッシュ後、ストリップは現在の湿度に対応する色で点灯します。センサーが存在しない場合、ストリップはオフ (実行器がデータを受け取りません) です。

ポータルのデバイス設定を開き、明るさ スライダーを調整します — ストリップは直ちに応答します。

## 次は?

- [05-rmt-command.md](05-rmt-command.md) — ポータル コマンド (RMT 出力) からアクチュエータを駆動します。
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — 実行器 API: ゾーン パルス、アニメーション、明るさ。
