# Ruban LED : allumer avec un effet et une couleur

Après cette page, la carte propose l'action **Turn on**, avec le choix de l'effet et de la couleur, et **Turn off**.

## Ce qu'il faut

- un ruban WS2812B (ou WS2811, SK6812) ;
- une résistance de 330–470 Ω sur la ligne de données ;
- une alimentation 5 V : une LED en blanc à pleine luminosité consomme jusqu'à 60 mA, 60 LED jusqu'à 3,6 A ;
- dans `platformio.ini` :

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

FastLED 3.10.5 avec le noyau ne tient pas dans la flash de l'ESP32-C3 (1,7 Mo pour une partition de 1,25 Mo) ; avec 3.10.3, c'est 1,16 Mo.

!!! warning
    Alimentez le ruban par une alimentation 5 V séparée et reliez les masses de l'alimentation et de la carte. Une broche de la carte ne suffit que pour un essai avec quelques LED.

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

// Image du ruban toutes les 20 ms : la couleur, luminosité douce pour breathe.
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

## Comment ça marche

- `card().action(id, mode, cb)` déclare une action. `mode` est le mode de l'unité après elle : `LIGHT_ANIMATION` pour l'allumage, `IDLE` pour l'extinction.
- `.name("ru", …)` et `.name("en", …)` sont le nom du bouton ; dans les autres langues, la carte affiche le nom anglais.
- `.select(...)` et `.color(...)` sont les paramètres de l'allumage. Le portail et l'application nomment eux-mêmes les purposes `effect` et `rgb_color` ; une valeur hors liste est remplacée par le noyau par la valeur par défaut.
- Le callback reçoit les paramètres dans `args` par leurs id, règle `status.mode` et appelle `publishStatusNow()`.
- La carte choisit selon le mode de l'unité : éteint, elle montre le formulaire **Turn on** ; allumé, le bloc du ruban allumé et le bouton **Turn off**.

## Vérification

Sur la carte, choisissez un effet et une couleur puis touchez **Turn on** : le ruban s'allume et la carte affiche **Turn off**.

## Suite

[Actions sans mode](05-actions.md).
