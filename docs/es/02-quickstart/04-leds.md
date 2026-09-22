# Tira LED: encender con un efecto y un color

Después de esta página la tarjeta tiene la acción **Turn on**, con elección de efecto y color, y **Turn off**.

## Qué necesitas

- una tira WS2812B (o WS2811, SK6812);
- una resistencia de 330–470 Ω en la línea de datos;
- una fuente de 5 V: un LED en blanco a pleno brillo consume hasta 60 mA, 60 LED hasta 3,6 A;
- en `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 junto con el núcleo no cabe en la flash del ESP32-C3 (1,7 MB para una partición de 1,25 MB); con 3.10.3 son 1,16 MB.

!!! warning
    Alimenta la tira desde una fuente de 5 V aparte y une las masas de la fuente y de la placa. Un pin de la placa solo sirve para probar con unos pocos LED.

## Código

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

// Fotograma de la tira cada 20 ms: el color, brillo suave para breathe.
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

## Cómo funciona

- `card().action(id, mode, cb)` declara una acción. `mode` es el modo de la unidad después de ella: `LIGHT_ANIMATION` al encender, `IDLE` al apagar.
- `.name("ru", …)` y `.name("en", …)` son el nombre del botón; en otros idiomas la tarjeta muestra el inglés.
- `.select(...)` y `.color(...)` son los parámetros del encendido. El portal y la aplicación rotulan solos los purposes `effect` y `rgb_color`; un valor fuera de la lista lo sustituye el núcleo por el valor por defecto.
- El callback recibe los parámetros en `args` por sus id, fija `status.mode` y llama a `publishStatusNow()`.
- La tarjeta elige según el modo de la unidad: apagada, muestra el formulario **Turn on**; encendida, el bloque de la tira encendida y el botón **Turn off**.

## Comprobación

En la tarjeta elige un efecto y un color y toca **Turn on**: la tira se enciende y la tarjeta muestra **Turn off**.

## Siguiente

[Acciones sin modo](05-actions.md).
