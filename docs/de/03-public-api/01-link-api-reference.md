# Öffentliche API: iDryer::Link

`iDryer::Link` ist der einzelne Einstiegspunkt für den Embedded-Entwickler. Die Fassade verbirgt den gesamten SDK-Stack: WiFi/Improv, Cloud-State-Machine, HTTP Claim, MQTT, lokales WebSocket, NVS. Das Produkt muss nur `telemetry`/`status` Felder füllen, Callbacks registrieren und `begin()`/`loop()` aufrufen.

---

## Lebenszyklus

Typisches `main.cpp` Gerüst:

```cpp
#include <idryer.h>
#include <runtime/idryer_runtime.h>  // only needed if setCommandHandler is used

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — strictly AFTER begin(): begin() installs its own dispatcher
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## Konfiguration: `iDryer::Config`

Wird einmal in `main.cpp` gefüllt, an den `Link` Konstruktor übergeben. Alle Felder verwenden Aggregate Init (C++ designierte Initialisierer).

| Feld | Typ | Zweck | Anmerkung |
|-------|------|---------|------|
| `deviceType` | `DeviceType` | Gerätetyp | **erforderlich** |
| `unitsCount` | `uint8_t` | Anzahl der Einheiten (Kammern), 1..`MAX_UNITS` (4) | **erforderlich** |
| `hasAirTemp` | `bool` | Lufttemperatursensor vorhanden | false = Feld wird aus JSON weggelassen |
| `hasAirHumidity` | `bool` | Feuchtigkeitssensor vorhanden | false = Feld wird aus JSON weggelassen |
| `hasHeaterTemp` | `bool` | Heizungstemperatursensor vorhanden | — |
| `hasHeaterPower` | `bool` | Heizungsleistungssensor vorhanden | — |
| `hasFanStatus` | `bool` | Ventilatorstatus vorhanden | — |
| `hasScales` | `bool` | Waagen vorhanden | — |
| `hasRfid` | `bool` | RFID-Lesegerät vorhanden | — |
| `allowHa` | `bool` | Home Assistant Integration erlauben | false = SDK erstellt keinen Client |
| `allowBambu` | `bool` | Bambu Lab LAN Integration erlauben | — |
| `allowMoonraker` | `bool` | Moonraker/Klipper Integration erlauben | — |
| `telemetryPeriodMs` | `uint32_t` | Auto-Publish-Intervall für `Telemetry` (ms) | 0 = nicht veröffentlichen |
| `statusPeriodMs` | `uint32_t` | Auto-Publish-Intervall für `Status` (ms) | 0 = nicht veröffentlichen |
| `hardwareVersion` | `const char*` | Hardwareversions-String | **erforderlich** |
| `firmwareVersion` | `const char*` | Firmwareversions-String | **erforderlich** |

---

## Klasse `iDryer::Link`

### Konstruktor

```cpp
explicit Link(const Config& cfg);
```

Nimmt die Konfiguration als const Referenz. `CFG` muss für die volle Objektlebensdauer existieren (typischerweise `static const`).

### Methoden

#### `begin()`

```cpp
bool begin();
```

Aktiviert den gesamten SDK-Stack: WiFi/Improv, Cloud-State-Machine, HTTP Claim, MQTT, lokales WebSocket, NVS Persistierung.

Einmal in `setup()` aufrufen. Gibt `true` bei erfolgreicher Initialisierung zurück.
