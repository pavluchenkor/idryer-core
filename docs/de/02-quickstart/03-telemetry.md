# Schritt 03 — Telemetrie: Sensordaten veröffentlichen

Nach diesem Schritt liest der ESP32 Temperatur und Luftfeuchtigkeit von einem SHT31-Sensor und veröffentlicht die Werte alle 10 Sekunden im Portal. Das Portal zeigt sie als Live-Grafik an.

## Was Sie benötigen

**Hardware:**

- SHT31 auf einem I2C-Breakout-Modul (Adresse 0x44 oder 0x45)
- Drähte: SDA, SCL, VCC (3,3 V), GND

**Software:**

- PlatformIO
- Bibliothek `robtillaart/SHT31 @ ^0.5.0`

## Schritte

**1. Verbinden Sie SHT31 mit ESP32-C3** (Standard-Pins, die von Storage Link verwendet werden):

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3.3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    Verbinden Sie den Sensor nur bei ausgeschaltetem Board.

**2. Fügen Sie die Bibliothek** zu `platformio.ini` hinzu:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... andere Abhängigkeiten
```

**3. Schließen Sie Wire und den Sensor** in `main.cpp` ein. Basierend auf [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp
#include <Wire.h>
#include "storage/sensors/Sht31ClimateSensor.h"

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. Initialisieren** in `setup()`:

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // auto-detects address 0x44 or 0x45
```

`begin()` gibt `false` zurück, wenn kein Sensor gefunden wird. Das Gerät läuft ohne ihn weiter.

**5. Rufen Sie `tick()` in `loop()` auf und aktualisieren Sie die Telemetrie-Felder:**

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airTempC[0]       = r.temperature;
        s_link.telemetry.airHumidityPct[0] = r.humidity;
    }
}
```

Die Bibliothek veröffentlicht automatisch alle `telemetry.*` Felder im Intervall, das durch `telemetryPeriodMs` in `iDryer::Config` gesetzt ist. Der Standard ist 10 000 ms.

**6. Aktivieren Sie die Fähigkeit in `iDryer::Config`:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```

## Überprüfung

Öffnen Sie Serial Monitor. Bei erfolgreichem Sensor-Erkennungsmeldung:

```
[MAIN] SHT31 at 0x44
```

Navigieren Sie im Portal zur Geräteseite — Temperatur- und Feuchtigkeitswerte werden alle 10 Sekunden aktualisiert.

Wenn der Sensor nicht gefunden wird, wird eine Warnung protokolliert und das Gerät läuft weiter. Überprüfen Sie, dass die Adresse 0x44/0x45 nicht von einem anderen Gerät auf dem Bus verwendet wird.

## Nächste Schritte

- [04-leds.md](04-leds.md) — visualisieren Sie Luftfeuchtigkeit mit LED-Streifen-Farbe.
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — Sensor-Implementierung.
