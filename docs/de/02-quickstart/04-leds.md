# LED-Streifen: Einschalten mit Effekt und Farbe

Nach dieser Seite hat die Karte die Aktion **Turn on** mit Auswahl von Effekt und Farbe sowie **Turn off**.

## Was Sie brauchen

- einen Streifen WS2812B (oder WS2811, SK6812);
- einen Widerstand 330–470 Ω in der Datenleitung;
- ein 5-V-Netzteil: eine LED zieht bei vollem Weiß bis zu 60 mA, 60 LEDs bis zu 3,6 A;
- in `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 passt zusammen mit dem Kern nicht in den Flash des ESP32-C3 (1,7 MB bei einer Partition von 1,25 MB); mit 3.10.3 sind es 1,16 MB.

!!! warning
    Den Streifen aus einem eigenen 5-V-Netzteil versorgen und die Massen von Netzteil und Board verbinden. Ein Pin des Boards reicht nur für einen Test mit wenigen LEDs.

## Code

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

// Streifen-Frame alle 20 ms: die Farbe, bei breathe weiche Helligkeit.
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

## So funktioniert es

- `card().action(id, mode, cb)` deklariert eine Aktion. `mode` ist der Modus der Einheit danach: `LIGHT_ANIMATION` beim Einschalten, `IDLE` beim Ausschalten.
- `.name("ru", …)` und `.name("en", …)` sind der Name der Taste; in anderen Sprachen zeigt die Karte den englischen.
- `.select(...)` und `.color(...)` sind die Parameter des Einschaltens. Die Purposes `effect` und `rgb_color` beschriften Portal und App selbst; einen Wert außerhalb der Liste ersetzt der Kern durch den Standardwert.
- Der Callback bekommt die Parameter in `args` nach ihren IDs, setzt `status.mode` und ruft `publishStatusNow()`.
- Die Karte wählt nach dem Modus der Einheit: aus zeigt das Formular **Turn on**; an zeigt den Block des leuchtenden Streifens und die Taste **Turn off**.

## Prüfen

Wählen Sie auf der Karte Effekt und Farbe und tippen Sie auf **Turn on**: der Streifen leuchtet, die Karte zeigt **Turn off**.

## Weiter

[Aktionen ohne Modus](05-actions.md).
