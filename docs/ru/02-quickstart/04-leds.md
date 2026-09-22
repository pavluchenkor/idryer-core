# LED-лента: включение с эффектом и цветом

После этой страницы на карточке появятся действия **Включить** — с выбором эффекта и цвета — и **Выключить**.

## Что понадобится

- лента WS2812B (или WS2811, SK6812);
- резистор 330–470 Ом в линию данных;
- блок питания 5 В: светодиод на полной яркости белого берёт до 60 мА, 60 светодиодов — до 3,6 А;
- в `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 вместе с ядром не помещается во флеш ESP32-C3 (1,7 МБ при разделе 1,25 МБ); с 3.10.3 — 1,16 МБ.

!!! warning
    Ленту питайте от отдельного блока 5 В, земли блока и платы соедините. От вывода платы — только проверка на нескольких светодиодах.

## Код

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

// Кадр ленты раз в 20 мс: цвет, для breathe — плавная яркость.
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

## Как это работает

- `card().action(id, mode, cb)` объявляет действие. `mode` — режим юнита после него: `LIGHT_ANIMATION` у включения, `IDLE` у выключения.
- `.name("ru", …)` и `.name("en", …)` — название кнопки; на других языках карточка показывает английское.
- `.select(...)` и `.color(...)` — параметры включения. `purpose` `effect` и `rgb_color` портал и приложение подписывают сами; значение не из списка ядро заменяет значением по умолчанию.
- Колбэк получает параметры в `args` по их id, выставляет `status.mode` и вызывает `publishStatusNow()`.
- Карточка выбирает по режиму юнита: не горит — форма **Включить**; горит — блок включённой подсветки и кнопка **Выключить**.

## Проверка

На карточке выберите эффект и цвет, нажмите **Включить** — лента загорится, карточка покажет **Выключить**.

## Что дальше

[Действия без режима](05-actions.md).
