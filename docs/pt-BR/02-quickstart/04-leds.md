# Fita LED: ligar com um efeito e uma cor

Depois desta página, o card tem a ação **Turn on**, com escolha de efeito e cor, e **Turn off**.

## O que você precisa

- uma fita WS2812B (ou WS2811, SK6812);
- um resistor de 330–470 Ω na linha de dados;
- uma fonte de 5 V: um LED em branco no brilho máximo puxa até 60 mA, 60 LEDs até 3,6 A;
- no `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ 3.10.3
```

O FastLED 3.10.5 junto com o núcleo não cabe na flash do ESP32-C3 (1,7 MB para uma partição de 1,25 MB); com o 3.10.3 são 1,16 MB.

!!! warning
    Alimente a fita com uma fonte de 5 V separada e ligue os terras da fonte e da placa. Um pino da placa só serve para testar com poucos LEDs.

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

// Quadro da fita a cada 20 ms: a cor, brilho suave no breathe.
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

## Como funciona

- `card().action(id, mode, cb)` declara uma ação. `mode` é o modo da unidade depois dela: `LIGHT_ANIMATION` ao ligar, `IDLE` ao desligar.
- `.name("ru", …)` e `.name("en", …)` são o nome do botão; nos outros idiomas o card mostra o inglês.
- `.select(...)` e `.color(...)` são os parâmetros de ligar. O portal e o app dão nome sozinhos aos purposes `effect` e `rgb_color`; um valor fora da lista o núcleo troca pelo valor padrão.
- O callback recebe os parâmetros em `args` pelos ids, define `status.mode` e chama `publishStatusNow()`.
- O card escolhe pelo modo da unidade: desligada mostra o formulário **Turn on**; ligada mostra o bloco da fita acesa e o botão **Turn off**.

## Verificação

No card escolha um efeito e uma cor e toque em **Turn on**: a fita acende e o card mostra **Turn off**.

## Próximo passo

[Ações sem modo](05-actions.md).
