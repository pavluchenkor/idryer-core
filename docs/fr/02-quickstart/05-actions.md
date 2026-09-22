# Actions sans mode : un relais et l'appel de l'appareil

Après cette page, la carte propose le formulaire **Ventilate** avec une durée en minutes et, dans l'en-tête, un bouton d'appel de l'appareil.

## Ce qu'il faut

- un module relais commandé en 3,3 V (ou un interrupteur à transistor) sur la broche 5 ;
- un ventilateur ou une autre charge sur le relais.

!!! warning
    Une charge sur le secteur 230 V exige isolation, fusible et boîtier. Sans expérience du secteur, montez avec une charge basse tension.

## Code

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // cellule « ventilateur marche/arrêt »
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 : relais coupé
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // un changement marche/arrêt est publié tout de suite
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // toutes les 250 ms
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
    // Sans mode : l'unité ne devient pas « occupée », le formulaire reste sur la carte.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Bouton d'appel de l'appareil, dans l'en-tête de la carte.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## Comment ça marche

- Une action sans mode (`mode` = `nullptr`) ne rend pas l'unité « occupée » : la carte n'affiche pas de bloc de session, le formulaire reste en place.
- Le purpose `duration` est une durée en minutes : le champ reçoit l'étiquette **Temps**, les limites viennent de `.param(...)`.
- `hasFan` donne la cellule « ventilateur marche/arrêt ». Le noyau publie tout de suite un changement de `telemetry.fanOn`, sans attendre la période.
- `.deviceClass("identify")` est le bouton permanent d'appel de l'appareil dans l'en-tête de la carte ; `clear_errors` est la remise à zéro des erreurs.

## Vérification

Sur la carte, réglez une durée et touchez **Ventilate** : le relais s'enclenche, la cellule du ventilateur indique « marche ». Le bouton d'appel de l'en-tête fait clignoter la LED de la carte.

## Suite

[Chauffage par PWM](06-pwm.md).
