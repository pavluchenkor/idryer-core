# Telemetrie: ein Sensor auf der Karte

Nach dieser Seite liest das Gerät Temperatur und Feuchte aus einem SHT31, und die Karte zeigt deren Zellen sowie einen eigenen Wert, den Taupunkt.

## Was Sie brauchen

- ein SHT31-Modul (I2C, Adresse 0x44 oder 0x45);
- Leitungen: SDA, SCL, 3,3 V, GND;
- in `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Den Sensor nur bei ausgeschaltetem Board anschließen.

## Code

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // Temperaturzelle
    .hasAirHumidity  = true,    // Feuchtezelle
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // Adresse 0x44 oder 0x45, je nach Brücke am Modul

// Taupunkt nach der Magnus-Formel: Beispiel für einen eigenen Wert.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // Keine Daten = NAN: das Feld wird nicht gesendet, die Karte zeigt „—“.
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL: Pins Ihres Boards
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // Abfrage alle 2 s
}

void loop() {
    s_link.loop();
}
```

## So funktioniert es

- `hasAirTemp` und `hasAirHumidity` in `Config` liefern Zellen für Temperatur und Feuchte auf der Karte. Code im Portal ist nicht nötig.
- Die Felder `s_link.telemetry.*` veröffentlicht der Kern selbst: alle 30 s, solange mindestens eine Einheit arbeitet, sonst alle 60 s. Die Perioden ändern die Felder `Config.telemetryPeriodMs` und `telemetryPeriodIdleMs`; null bedeutet den Wert aus dem Vertrag.
- `NAN` bedeutet keine Daten: das Feld wird nicht veröffentlicht. Keine Null einsetzen, sonst zeigt das Diagramm einen falschen Einbruch.
- Eigener Wert: `onTelemetryPublish` fügt vor der Veröffentlichung ein Feld in die Telemetrie ein, `card().sensor(id, label, unit, path, deviceClass)` deklariert es auf der Karte. `path` ist der Pfad in der Telemetrie; `deviceClass` ist optional und legt Symbol und Format fest.
- `s_link.every(ms, fn)` ruft eine Funktion aus `loop()` mit der angegebenen Periode auf, ohne die Verbindung zu blockieren.

## Prüfen

Innerhalb einer Minute nach dem Flashen zeigt die Karte Temperatur, Feuchte und Taupunkt. Ohne Sensor bleiben die Zellen leer, das Gerät arbeitet weiter.

## Weiter

[LED-Streifen](04-leds.md).
