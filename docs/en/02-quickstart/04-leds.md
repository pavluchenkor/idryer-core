# LED strip: turning on with an effect and a color

After this page the card has the **Turn on** action, with a choice of effect and color, and **Turn off**.

## What you need

- a WS2812B strip (or WS2811, SK6812);
- a 330–470 Ω resistor in the data line;
- a 5 V power supply: one LED at full white brightness draws up to 60 mA, 60 LEDs up to 3.6 A;
- in `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 together with the core does not fit into ESP32-C3 flash (1.7 MB for a 1.25 MB partition); with 3.10.3 it is 1.16 MB.

!!! warning
    Power the strip from a separate 5 V supply and connect the grounds of the supply and the board. A board pin is only for a test with a few LEDs.

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

// Strip frame every 20 ms: the color, smooth brightness for breathe.
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

## How it works

- `card().action(id, mode, cb)` declares an action. `mode` is the unit mode after it: `LIGHT_ANIMATION` for turning on, `IDLE` for turning off.
- `.name("ru", …)` and `.name("en", …)` are the button name; in other languages the card shows the English one.
- `.select(...)` and `.color(...)` are the turn-on parameters. The portal and the app label the `effect` and `rgb_color` purposes themselves; a value not in the list is replaced by the core with the default.
- The callback gets the parameters in `args` by their ids, sets `status.mode` and calls `publishStatusNow()`.
- The card chooses by the unit mode: off shows the **Turn on** form; on shows the lit-strip block and the **Turn off** button.

## Check

On the card choose an effect and a color and tap **Turn on**: the strip lights up and the card shows **Turn off**.

## Next

[Actions without a mode](05-actions.md).
