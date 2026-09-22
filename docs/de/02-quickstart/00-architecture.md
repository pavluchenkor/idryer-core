# Wie idryer-core aufgebaut ist

idryer-core ist eine Bibliothek für ESP32. Sie übernimmt alles, was ein Gerät mit dem Portal und der App verbindet:

- WLAN: das Netz kommt aus der iDryer-App (ESPTouch) oder von einer Webseite über USB (Improv);
- Kopplung mit einem Konto per Einmal-Token und Entkopplung;
- eine sichere MQTT-Sitzung mit Wiederverbindung;
- Zugriff im lokalen Netz: die App steuert das Gerät auch ohne Internet;
- Veröffentlichung von Telemetrie und Status, Zustellung von Befehlen;
- Firmware-Updates über das Netz;
- die Gerätekarte im Portal und in der App.

Sie schreiben nur Ihren Teil: Sensoren lesen, Lasten ansteuern, festlegen, was die Karte zeigt und welche Vorgänge sie startet.

## Ein Einstiegspunkt: `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // eigenes Gerät
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // Temperaturzelle
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // Ihr Code
}
```

| Sie | Der Kern |
|---|---|
| füllen `s_link.telemetry.*` | veröffentlicht alle 30 s, im Leerlauf alle 60 s |
| ändern `s_link.status.*` (Modus, Sollwert, Zeit) und rufen `publishStatusNow()` | stellt den Status zu; die Karte wechselt nach Modus |
| deklarieren `s_link.card()`: Sensoren, Bedienelemente, Aktionen | baut das Card-Manifest und veröffentlicht es |
| registrieren `s_link.onCommand(...)` | leitet Befehle aus der Cloud und aus dem lokalen Netz weiter |

## Gerätekarte

Portal und App zeichnen die Karte nach dem Card-Manifest, das das Gerät sendet:

- die Flags `Config.has*` liefern fertige Zellen: Temperatur, Feuchte, Heizleistung, Lüfter und weitere;
- `card().sensor(...)` fügt einen eigenen Wert über seinen Pfad in der Telemetrie hinzu;
- `card().action(...)` deklariert einen Vorgang: den Modus der Einheit danach und die Startparameter.

Code im Portal ist dafür nicht nötig. Details: [Gerätekarte: das Card-Manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml` ist die Quelle der Wahrheit

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) beschreibt das Protokoll: Topics, Felder von Telemetrie und Status, Fähigkeiten der Geräte, das Card-Manifest. Daraus werden erzeugt:

| Was | Wohin |
|---|---|
| `iDryer::Config` (Flags `has*`) und API-Strukturen | `src/_generated/iDryer_api.h` |
| MQTT-Topics | `contracts/_generated/mqtt_topics.h` |
| UART-Protokoll ESP32 ↔ Controller | `contracts/_generated/uart_protocol.h` |
| TypeScript-Typen für das Portal | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Dateien in `_generated/` nicht von Hand bearbeiten: `contracts/regen.sh` überschreibt sie aus dem Vertrag.

Ein eigener Wert braucht keine Vertragsänderung: ihn deklariert `card().sensor(...)`. Der Vertrag ändert sich, wenn alle Produkte eine neue Fähigkeit brauchen: erst `mqtt_contract.yaml`, dann `regen.sh`, dann der Code.

## Produkte auf dem Kern

- **iDryer Link**: Kommunikationsmodul des Trockners, ein ESP32 neben dem Controller, Austausch über UART.
- **iDryer Storage**: Beleuchtung eines Spulenregals, adressierbarer Streifen und SHT31-Sensor.
- **iHeater Link**: Steuerung des iHeater-Heizers, Integrationen mit Bambu Lab, Klipper/Moonraker und Home Assistant.

## Weiter

[In 5 Minuten starten](01-five-minutes.md).
