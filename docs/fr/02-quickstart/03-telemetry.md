# Télémétrie : un capteur sur la carte

Après cette page, l'appareil lit la température et l'humidité d'un SHT31, et la carte affiche leurs cellules ainsi qu'une valeur à vous, le point de rosée.

## Ce qu'il faut

- un module SHT31 (I2C, adresse 0x44 ou 0x45) ;
- des fils : SDA, SCL, 3,3 V, GND ;
- dans `platformio.ini` :

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Branchez le capteur carte hors tension.

## Code

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // cellule de température
    .hasAirHumidity  = true,    // cellule d'humidité
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // adresse 0x44 ou 0x45, selon le cavalier du module

// Point de rosée par la formule de Magnus : exemple de valeur à vous.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // Pas de données = NAN : le champ n'est pas envoyé, la carte affiche « — ».
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL : broches de votre carte
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // lecture toutes les 2 s
}

void loop() {
    s_link.loop();
}
```

## Comment ça marche

- `hasAirTemp` et `hasAirHumidity` dans `Config` donnent les cellules de température et d'humidité sur la carte. Aucun code n'est nécessaire sur le portail.
- Le noyau publie lui-même les champs `s_link.telemetry.*` : toutes les 30 s tant qu'au moins une unité travaille, sinon toutes les 60 s. Les champs `Config.telemetryPeriodMs` et `telemetryPeriodIdleMs` changent les périodes ; zéro signifie la valeur du contrat.
- `NAN` signifie pas de données : le champ n'est pas publié. Ne mettez pas zéro à la place, sinon le graphique montre une fausse chute.
- Une valeur à vous : `onTelemetryPublish` ajoute un champ à la télémétrie avant publication, `card().sensor(id, label, unit, path, deviceClass)` la déclare sur la carte. `path` est le chemin dans la télémétrie ; `deviceClass` est facultatif et fixe l'icône et le format.
- `s_link.every(ms, fn)` appelle une fonction depuis `loop()` avec la période donnée, sans bloquer la liaison.

## Vérification

Dans la minute qui suit le flashage, la carte affiche la température, l'humidité et le point de rosée. Sans capteur, les cellules restent vides et l'appareil continue de fonctionner.

## Suite

[Ruban LED](04-leds.md).
