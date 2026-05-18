# 步驟 04 — 指示：由傳感器數據驅動的 LED 條帶

完成此步驟後，WS2812B 條帶將根據濕度改變顏色，亮度將可通過 `set` 命令從門戶控制。

## 您需要什麼

**硬件：**

- WS2812B LED 條帶（或 WS2811/SK6812）
- 數據線上的 330–470 Ω 電阻
- 5 V 電源（電流取決於條帶長度；300 個 LED 可吸引高達 18 A）

**軟件：**

- 庫 `fastled/FastLED @ ^3.6.0`

!!! warning
    從專用 5 V 電源為條帶供電。通過主機板的 3.3 V 或 5 V 引腳供電僅適用於幾個 LED 的快速冒煙測試。

## 步驟

**1. 將 FastLED 添加**到 `platformio.ini`：

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... other dependencies
```

**2. 在 `main.cpp` 中聲明緩衝區和執行器**。基於 [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp)：

```cpp
#include <FastLED.h>
#include "storage/led_strip/led_strip_executor.h"

#define STORAGE_LED_PIN  4
#define STORAGE_MAX_LEDS 300

static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. 在 `setup()` 中初始化條帶**：

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

將 `60` 替換為條帶的實際 LED 數量。

**4. 在 `loop()` 中根據濕度改變顏色**。顏色刻度：藍色（干燥）→ 黃色 → 紅色（潮濕）：

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

**5. 從門戶控制亮度。** 在 `setup()` 中註冊一個 `set` 命令處理程序：

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

`MENU_BRIGHTNESS` 是來自 [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h) 的常數，由 `regen.sh` 從 `menu.yaml` 生成。在您自己的產品中，名稱和值將有所不同 — 檢查您項目的 `menu_ids.h`。

## 驗證

刷新後，條帶應根據當前濕度以相應的顏色點亮。如果沒有傳感器，條帶保持關閉（執行器未收到數據）。

打開門戶上的設備設置並調整亮度滑塊 — 條帶立即響應。

## 下一步

- [05-rmt-command.md](05-rmt-command.md) — 從門戶命令驅動執行器（RMT 輸出）。
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — 執行器 API：區域脈衝、動畫、亮度。
