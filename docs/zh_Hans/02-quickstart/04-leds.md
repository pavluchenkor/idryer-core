# 步骤 04 — 指示：由传感器数据驱动的 LED 条带

完成此步骤后，WS2812B 条带将根据湿度改变颜色，亮度将可通过 `set` 命令从门户控制。

## 您需要什么

**硬件：**

- WS2812B LED 条带（或 WS2811/SK6812）
- 数据线上的 330–470 Ω 电阻
- 5 V 电源（电流取决于条带长度；300 个 LED 可吸引高达 18 A）

**软件：**

- 库 `fastled/FastLED @ ^3.6.0`

!!! warning
    从专用 5 V 电源为条带供电。通过主板的 3.3 V 或 5 V 引脚供电仅适用于几个 LED 的快速冒烟测试。

## 步骤

**1. 将 FastLED 添加**到 `platformio.ini`：

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... other dependencies
```

**2. 在 `main.cpp` 中声明缓冲区和执行器**。基于 [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp)：

```cpp
#include <FastLED.h>
#include "storage/led_strip/led_strip_executor.h"

#define STORAGE_LED_PIN  4
#define STORAGE_MAX_LEDS 300

static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. 在 `setup()` 中初始化条带**：

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

将 `60` 替换为条带的实际 LED 数量。

**4. 在 `loop()` 中根据湿度改变颜色**。颜色刻度：蓝色（干燥）→ 黄色 → 红色（潮湿）：

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

**5. 从门户控制亮度。** 在 `setup()` 中注册一个 `set` 命令处理程序：

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

`MENU_BRIGHTNESS` 是来自 [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h) 的常数，由 `regen.sh` 从 `menu.yaml` 生成。在您自己的产品中，名称和值将有所不同 — 检查您项目的 `menu_ids.h`。

## 验证

刷新后，条带应根据当前湿度以相应的颜色点亮。如果没有传感器，条带保持关闭（执行器未收到数据）。

打开门户上的设备设置并调整亮度滑块 — 条带立即响应。

## 下一步

- [05-rmt-command.md](05-rmt-command.md) — 从门户命令驱动执行器（RMT 输出）。
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — 执行器 API：区域脉冲、动画、亮度。
