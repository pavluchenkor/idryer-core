# Chauffage par PWM : mode, session, puissance

Après cette page, la carte lance la chauffe avec une température et une durée, affiche la session et la puissance, et l'appareil maintient la température par une sortie PWM d'après le capteur.

## Ce qu'il faut

- le capteur SHT31 de l'[étape Télémétrie](03-telemetry.md) ;
- un MOSFET à niveau logique (s'ouvre à 3,3 V) sur la broche 3 et un élément chauffant adapté à sa tension d'alimentation ;
- dans `platformio.ini` : `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    Ne laissez pas sans surveillance un chauffage sans fusible thermique : le firmware peut se figer et le MOSFET peut claquer en court-circuit.

## Code

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // grille du MOSFET
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // rapport cyclique 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // cellule de puissance de chauffe
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "PWM Heater",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);

static void readSensor() {
    const bool ok = s_sht.read();
    s_link.telemetry.airTempC[0]       = ok ? s_sht.getTemperature() : NAN;
    s_link.telemetry.airHumidityPct[0] = ok ? s_sht.getHumidity()    : NAN;
}

static uint32_t s_startMs = 0;

static void stopHeat(uint8_t unit) {
    ledcWrite(PWM_CHANNEL, 0);
    s_link.telemetry.heaterPower01[unit] = 0.0f;
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 : sans limite
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Une fois par seconde : puissance selon l'écart à la consigne, durée de session.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // Pas de mesure = chauffage coupé : ne jamais chauffer à l'aveugle.
    float power = 0.0f;
    if (!isnan(t)) power = constrain((s_link.status.targetTempC[0] - t) * 0.1f, 0.0f, 1.0f);
    ledcWrite(PWM_CHANNEL, (uint32_t)(power * 255));
    s_link.telemetry.heaterPower01[0] = power;

    const uint32_t elapsed = (millis() - s_startMs) / 1000;
    s_link.status.elapsedS[0] = elapsed;
    if (s_link.status.durationS[0] && elapsed >= s_link.status.durationS[0]) stopHeat(0);
}

void setup() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(8, 9);
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");

    s_link.every(2000, readSensor);
    s_link.every(1000, regulate);
}

void loop() {
    s_link.loop();
}
```

## Comment ça marche

- Une action avec le mode `HEATING` lance une session : le callback règle le mode, la consigne et la durée et appelle `publishStatusNow()`. Selon le mode, la carte affiche le bloc de session et **Stop**.
- Le portail et l'application nomment eux-mêmes les purposes `target_temperature` et `duration`. Les nombres dans `args` sont déjà bornés aux limites du paramètre.
- `durationS = 0` signifie sans limite de temps ; la carte affiche `elapsedS` comme temps écoulé.
- `hasHeater` donne la cellule de puissance. En fonctionnement, le noyau envoie la moyenne de `heaterPower01` sur les deux dernières périodes de publication : les changements fréquents du PWM ne sautent pas sur la carte.
- Le régulateur est ici proportionnel : 10 °C sous la consigne donnent la pleine puissance. Pour un maintien précis, remplacez-le par un PID.
- `ledcSetup` / `ledcAttachPin` sont l'API d'arduino-esp32 2.x. En 3.x, utilisez `ledcAttach(pin, freq, bits)` et `ledcWrite(pin, duty)`.

## Vérification

Lancez **Heat** sur la carte : le bloc de session apparaît avec la consigne et la durée, la cellule de puissance montre la part de fonctionnement du chauffage. **Stop** coupe la sortie.

## Suite

- [Carte de l'appareil : le card manifest](../09-add-product/02-add-widget.md) : paramètres depuis le menu de l'appareil, profils, disposition de la carte.
- [Ajouter un nouveau produit](../09-add-product/01-add-new-product.md).
