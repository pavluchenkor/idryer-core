# LED pásek: zapnutí s efektem a barvou

Po této stránce bude mít karta akci **Turn on** s výběrem efektu a barvy a akci **Turn off**.

## Co budete potřebovat

- pásek WS2812B (nebo WS2811, SK6812);
- rezistor 330–470 Ω v datové lince;
- zdroj 5 V: jedna LED při plném jasu bílé bere až 60 mA, 60 LED až 3,6 A;
- v `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 se spolu s jádrem nevejde do flash paměti ESP32-C3 (1,7 MB při oddílu 1,25 MB); s 3.10.3 je to 1,16 MB.

!!! warning
    Pásek napájejte z vlastního zdroje 5 V a propojte země zdroje a desky. Pin desky stačí jen na zkoušku s několika LED.

## Kód

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

// Snímek pásku každých 20 ms: barva, u breathe plynulý jas.
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

## Jak to funguje

- `card().action(id, mode, cb)` deklaruje akci. `mode` je režim jednotky po ní: `LIGHT_ANIMATION` u zapnutí, `IDLE` u vypnutí.
- `.name("ru", …)` a `.name("en", …)` jsou název tlačítka; v ostatních jazycích karta ukáže anglický.
- `.select(...)` a `.color(...)` jsou parametry zapnutí. Purposes `effect` a `rgb_color` popíšou portál i aplikace samy; hodnotu mimo seznam jádro nahradí výchozí hodnotou.
- Callback dostane parametry v `args` podle jejich id, nastaví `status.mode` a zavolá `publishStatusNow()`.
- Karta volí podle režimu jednotky: vypnuto ukáže formulář **Turn on**; zapnuto ukáže blok rozsvíceného pásku a tlačítko **Turn off**.

## Kontrola

Na kartě vyberte efekt a barvu a klepněte na **Turn on**: pásek se rozsvítí a karta ukáže **Turn off**.

## Dál

[Akce bez režimu](05-actions.md).
