# Step 04 — Indication: LED strip driven by sensor data

After this step a WS2812B strip will change colour based on humidity, and brightness will be controllable from the portal via a `set` command.

## What you need

**Hardware:**

- WS2812B LED strip (or WS2811/SK6812)
- 330–470 Ω resistor on the data line
- 5 V power supply (current depends on strip length; 300 LEDs draw up to 18 A)

**Software:**

- Library `fastled/FastLED @ ^3.6.0`

!!! warning
    Power the strip from a dedicated 5 V supply. Powering through the board's 3.3 V or 5 V pin is acceptable only for a quick smoke test with a few LEDs.

## Steps

**1. Add FastLED** to `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... other dependencies
```

**2. Declare the buffer and executor** in `main.cpp`. Based on [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp
#include <FastLED.h>
#include "storage/led_strip/led_strip_executor.h"

#define STORAGE_LED_PIN  4
#define STORAGE_MAX_LEDS 300

static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. Initialise the strip** in `setup()`:

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

Replace `60` with the actual LED count for your strip.

**4. Change colour by humidity** in `loop()`. Colour scale: blue (dry) → yellow → red (humid):

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

**5. Control brightness from the portal.** Register a `set` command handler in `setup()`:

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

`MENU_BRIGHTNESS` is a constant from [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h), generated from `menu.yaml` via `regen.sh`. In your own product the name and value will differ — check `menu_ids.h` of your project.

## Verification

After flashing, the strip should light up in the colour corresponding to the current humidity. If no sensor is present, the strip stays off (executor receives no data).

Open the device settings on the portal and adjust the brightness slider — the strip responds immediately.

## What's next

- [05-rmt-command.md](05-rmt-command.md) — drive an actuator from a portal command (RMT output).
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — executor API: zone pulse, animations, brightness.
