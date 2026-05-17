# Einen Sensor hinzufügen

## Wann man dies verwenden sollte

Wenn das Gerät periodisch einen physischen Sensor auslesen muss (Temperatur, Luftfeuchtigkeit, Gewicht usw.) und die Messwerte an die Cloud oder einen LAN-Client veröffentlichen soll — verwenden Sie dieses Rezept.

## Schlüsselfertig-Code

Kopieren Sie in Ihr Projekt und ersetzen Sie `MyClimate` durch Ihren Klassennamen:

```cpp
// MyClimate.h — product sensor driver
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // non-blocking, no delay()
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
    // Publishing is automatic, on the telemetryPeriodMs timer from Config.
}
```

## Erklärung

Das Produkt füllt nur die `s_link.telemetry.*` Felder in `loop()` auf. Die Fassade veröffentlicht sie auf MQTT und Local WS alle `Config.telemetryPeriodMs` Millisekunden von selbst — es ist nicht nötig, `publishTelemetryNow()` manuell aufzurufen. Dies ist der Hauptunterschied zum manuellen MQTT: Keine `StaticJsonDocument`, keine `publishTelemetry`, keine separate Publisher-Klasse.

Wenn Sie Messwerte sofort außerhalb des Timers veröffentlichen müssen — rufen Sie `s_link.publishTelemetryNow()` auf.

Die Flags `hasAirTemp` / `hasAirHumidity` in `Config` steuern, welche Felder im JSON erscheinen. Ein Feld mit dem Flag `false` wird nicht veröffentlicht.

Vollständige Liste der Telemetriefelder: [Telemetrie-Felder](../03-public-api/01-link-api-reference.md#telemetry-fields).

## Vollständiges Beispiel im Repo

Referenzimplementierung: `Sht31ClimateSensor` + Füllung von `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]` in `iDryer-Storage/src/main.cpp`.
