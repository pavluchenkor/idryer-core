# Шаг 04 — Индикация: LED-лента по данным датчика

После этого шага WS2812B-лента будет менять цвет в зависимости от влажности, а яркостью можно управлять с портала командой `set`.

## Что понадобится

**Железо:**

- LED-лента WS2812B (или WS2811/SK6812)
- Резистор 330–470 Ом на линии данных
- Блок питания 5 В (ток зависит от длины ленты; 300 светодиодов — до 18 А)

**ПО:**

- Библиотека `fastled/FastLED @ ^3.6.0`

!!! warning
    Подключайте ленту к отдельному источнику питания 5 В. Питание через пин 3.3 В или 5 V платы допустимо только для проверки с несколькими светодиодами.

## Шаги

**1. Добавить FastLED** в `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... остальные зависимости
```

**2. Объявить буфер и executor** в `main.cpp`. Основано на [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp
#include <FastLED.h>
#include "storage/led_strip/led_strip_executor.h"

#define STORAGE_LED_PIN  4
#define STORAGE_MAX_LEDS 300

static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. Инициализировать ленту** в `setup()`:

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

Укажите реальное количество светодиодов вместо `60`.

**4. Менять цвет по влажности** в `loop()`. Цветовая шкала: синий (сухо) → жёлтый → красный (влажно):

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airHumidityPct[0] = r.humidity;

        // Влажность 20%–80% → оттенок от 160 (синий) до 0 (красный).
        float h = constrain(r.humidity, 20.0f, 80.0f);
        uint8_t hue = (uint8_t)(160.0f - (h - 20.0f) / 60.0f * 160.0f);
        fill_solid(s_leds, s_executor.ledsCount(), CHSV(hue, 255, 200));
        FastLED.show();
    }
}
```

**5. Управление яркостью с портала.** Зарегистрируйте обработчик команды `set` в `setup()`:

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

`MENU_BRIGHTNESS` — константа из [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h), сгенерированного из `menu.yaml` через `regen.sh`. В вашем продукте имя и значение будут свои — смотрите `menu_ids.h` своего проекта.

## Проверка

После прошивки лента должна засветиться цветом, соответствующим текущей влажности. При отсутствии датчика — лента остаётся тёмной (executor не получает данных).

На портале откройте настройки устройства и измените яркость ползунком — лента ответит немедленно.

## Что дальше

- [05-rmt-command.md](05-rmt-command.md) — управлять исполнительным механизмом командой с портала (RMT-выход).
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — API executor: зонные pulse, анимации, яркость.
