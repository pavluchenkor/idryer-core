# Akce bez režimu: relé a přivolání zařízení

Po této stránce bude mít karta formulář **Ventilate** s časem v minutách a v záhlaví tlačítko přivolání zařízení.

## Co budete potřebovat

- modul relé ovládaný z 3,3 V (nebo tranzistorový spínač) na pinu 5;
- ventilátor nebo jinou zátěž na relé.

!!! warning
    Zátěž na síti 230 V jen s izolací, pojistkou a v krabici. Bez zkušeností se síťovým napětím stavte s nízkonapěťovou zátěží.

## Kód

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // buňka „ventilátor zap/vyp“
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 = relé vypnuto
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // změna zap/vyp se publikuje hned
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // každých 250 ms
    if (s_ventUntilMs && (int32_t)(millis() - s_ventUntilMs) >= 0) {
        s_ventUntilMs = 0;
        digitalWrite(RELAY_PIN, LOW);
        s_link.telemetry.fanOn[0] = false;
    }
    if (s_blinks) digitalWrite(LED_PIN, --s_blinks % 2);
}

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // Bez režimu: jednotka se nestane „obsazenou“, formulář zůstane na kartě.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Tlačítko přivolání zařízení v záhlaví karty.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## Jak to funguje

- Akce bez režimu (`mode` = `nullptr`) neudělá jednotku „obsazenou“: karta neukáže blok relace a formulář zůstane na místě.
- Purpose `duration` je čas v minutách: pole dostane popisek **Čas**, meze pocházejí z `.param(...)`.
- `hasFan` dává buňku „ventilátor zap/vyp“. Změnu `telemetry.fanOn` jádro publikuje hned, nečeká na periodu.
- `.deviceClass("identify")` je stálé tlačítko přivolání zařízení v záhlaví karty; `clear_errors` je vymazání chyb.

## Kontrola

Na kartě nastavte čas a klepněte na **Ventilate**: relé sepne a buňka ventilátoru ukáže „zap“. Tlačítko přivolání v záhlaví rozbliká LED desky.

## Dál

[Topení přes PWM](06-pwm.md).
