# Aktionen ohne Modus: ein Relais und Gerät rufen

Nach dieser Seite hat die Karte das Formular **Ventilate** mit einer Zeit in Minuten und im Kopf eine Taste zum Rufen des Geräts.

## Was Sie brauchen

- ein Relaismodul mit Ansteuerung ab 3,3 V (oder einen Transistorschalter) an Pin 5;
- einen Lüfter oder eine andere Last am Relais.

!!! warning
    Eine Last an 230 V Netzspannung nur mit Isolation, Sicherung und Gehäuse. Ohne Erfahrung mit Netzspannung mit einer Niedervoltlast aufbauen.

## Code

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // Zelle „Lüfter an/aus“
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 = Relais aus
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // ein Wechsel an/aus wird sofort veröffentlicht
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // alle 250 ms
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
    // Ohne Modus: die Einheit wird nicht „belegt“, das Formular bleibt auf der Karte.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Taste „Gerät rufen“ im Kartenkopf.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## So funktioniert es

- Eine Aktion ohne Modus (`mode` = `nullptr`) macht die Einheit nicht „belegt“: die Karte zeigt keinen Sitzungsblock, das Formular bleibt stehen.
- Der Purpose `duration` ist eine Zeit in Minuten: das Feld bekommt die Beschriftung **Zeit**, die Grenzen kommen aus `.param(...)`.
- `hasFan` liefert die Zelle „Lüfter an/aus“. Einen Wechsel von `telemetry.fanOn` veröffentlicht der Kern sofort, ohne auf die Periode zu warten.
- `.deviceClass("identify")` ist die feste Taste zum Rufen des Geräts im Kartenkopf; `clear_errors` ist das Zurücksetzen von Fehlern.

## Prüfen

Stellen Sie auf der Karte eine Zeit ein und tippen Sie auf **Ventilate**: das Relais schaltet ein, die Lüfterzelle zeigt „an“. Die Ruftaste im Kopf lässt die LED des Boards blinken.

## Weiter

[Heizen per PWM](06-pwm.md).
