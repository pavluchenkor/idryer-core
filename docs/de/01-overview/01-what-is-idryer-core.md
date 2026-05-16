# Was ist idryer-core

Wenn Sie ein ESP32-Gerät für die iDryer Cloud entwickeln, handhabt diese Bibliothek WiFi-Bereitstellung (Improv), das Claim-Protokoll, die MQTT-Sitzung (TLS, erneute Verbindung, Zeitsynchronisierung), periodische Telemetrie-/Statusveröffentlichung und eingehende Befehlsweiterleitung. Etwa 500 Zeilen Boilerplate-Code fallen in `link.begin(); link.loop();` zusammen.

## Minimales Beispiel

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## Was die Bibliothek tut

- WiFi-Verbindung und Keep-Alive; Improv-Bereitstellung über Web Serial für die anfängliche Einrichtung.
- Claim-Protokoll: Geräteregistrierung im Backend, Kontoanspruch über PIN.
- MQTT-Sitzung mit dem iDryer Broker: TLS, persistente Sitzung, Auto-Neuverbindung, NTP-Zeitsynchronisierung.
- Periodische Veröffentlichung von Telemetrie (`Telemetry`) und Status (`Status`) nach Zeitplan.
- Weiterleitung eingehender Befehle (`commands/invoke`, `commands/set`, `commands/ping`) zum Produkthandler.
- Lokaler WebSocket-Server: Ein LAN-Client sieht den gleichen Stream wie die Cloud.
- NVS-Persistierung: WiFi-Anmeldedaten, Gerätetoken, Menükonfiguration über Neustarts hinweg.

## Was die Bibliothek nicht tut

- Verwaltet keine Produkt-Hardware: Ventilatoren, Heizer, LED-Streifen, Sensoren.
- Enthält keine Geschäftslogik für Trocknen, Lagern oder Beleuchtung.
- Kennt produktspezifische Menüparameter nicht — transportiert sie nur.
- Veröffentlicht Telemetrie nicht ohne Daten vom Produkt: Sie füllen `link.telemetry.*` selbst in `loop()` aus.

## Nächste Schritte

- [Erste Schritte in 5 Minuten](../02-quickstart/01-five-minutes.md)
- [Vollständige API: iDryer::Link](../03-public-api/01-link-api-reference.md)
