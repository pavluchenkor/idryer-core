# Telemetrie: senzor na kartě

Po této stránce zařízení čte teplotu a vlhkost ze SHT31 a karta ukáže jejich buňky i vlastní veličinu, rosný bod.

## Co budete potřebovat

- modul SHT31 (I2C, adresa 0x44 nebo 0x45);
- vodiče: SDA, SCL, 3,3 V, GND;
- v `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Senzor připojujte při vypnutém napájení desky.

## Kód

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // buňka teploty
    .hasAirHumidity  = true,    // buňka vlhkosti
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // adresa 0x44 nebo 0x45 podle propojky modulu

// Rosný bod podle Magnusova vzorce: příklad vlastní veličiny.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // Žádná data = NAN: pole se neodešle, karta ukáže „—“.
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL: piny vaší desky
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // čtení každé 2 s
}

void loop() {
    s_link.loop();
}
```

## Jak to funguje

- `hasAirTemp` a `hasAirHumidity` v `Config` dávají na kartě buňky teploty a vlhkosti. Kód na portálu není potřeba.
- Pole `s_link.telemetry.*` jádro publikuje samo: každých 30 s, dokud pracuje aspoň jedna jednotka, jinak každých 60 s. Periody mění pole `Config.telemetryPeriodMs` a `telemetryPeriodIdleMs`; nula znamená hodnotu z kontraktu.
- `NAN` znamená žádná data: pole se nepublikuje. Nedosazujte nulu, jinak graf ukáže falešný propad.
- Vlastní veličina: `onTelemetryPublish` přidá pole do telemetrie před publikací, `card().sensor(id, label, unit, path, deviceClass)` ho deklaruje na kartě. `path` je cesta v telemetrii; `deviceClass` je nepovinný a určuje ikonu a formát.
- `s_link.every(ms, fn)` volá funkci z `loop()` se zadanou periodou a neblokuje spojení.

## Kontrola

Do minuty po nahrání ukáže karta teplotu, vlhkost a rosný bod. Bez senzoru zůstanou buňky prázdné a zařízení pracuje dál.

## Dál

[LED pásek](04-leds.md).
