# Přidání senzoru

## Kdy použít

Pokud zařízení potřebuje periodicky číst fyzický senzor (teplota, vlhkost, váha, atd.) a publikovat údaje do cloudu nebo LAN klienta — použij tento návod.

## Hotový kód

Zkopíruj do svého projektu a nahraď `MyClimate` názvem své třídy:

```cpp
// MyClimate.h — ovladač senzoru produktu
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // bez blokování, bez delay()
    float temperature() const;
    float humidity()    const;
    bool  ok()          const;
};
```

```cpp
// main.cpp
#include <iDryer.h>
#include "MyClimate.h"

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link  s_link(CFG);
static MyClimate     s_climate;
static bool          s_sensorOk = false;

void setup() {
    s_sensorOk = s_climate.begin();
    s_link.begin();
}

void loop() {
    s_link.loop();

    if (s_sensorOk) {
        s_climate.tick(millis());
        if (s_climate.ok()) {
            s_link.telemetry.airTempC[0]       = s_climate.temperature();
            s_link.telemetry.airHumidityPct[0] = s_climate.humidity();
        }
    }
    // Publikování je automatické, na časovači telemetryPeriodMs z Config.
}
```

## Vysvětlení

Produkt pouze vyplňuje pole `s_link.telemetry.*` v `loop()`. Fasáda je publikuje do MQTT a Local WS každých `Config.telemetryPeriodMs` milisekund sama — není třeba ručně volat `publishTelemetryNow()`. Toto je klíčový rozdíl od manuálního MQTT: žádné `StaticJsonDocument`, žádné `publishTelemetry`, žádna samostatná třída publisheru.

Pokud potřebuješ publikovat údaje okamžitě mimo časovač — zavolej `s_link.publishTelemetryNow()`.

Příznaky `hasAirTemp` / `hasAirHumidity` v `Config` řídí, která pole se objevují v JSON. Pole, jehož příznak je `false`, se nepublikuje.

Úplný seznam telemetrických polí: [Telemetrická pole](../03-public-api/01-link-api-reference.md#telemetry-fields).

## Plný příklad v repo

Referenční implementace: `Sht31ClimateSensor` + vyplnění `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]` v `iDryer-Storage/src/main.cpp`.
