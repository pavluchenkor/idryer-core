# Wie idryer-core funktioniert

idryer-core ist eine Bibliothek für ESP32, die den gesamten Cloud-Stack handhabt: WiFi-Bereitstellung über Improv-Serial, das Claim-Protokoll zum Binden eines Geräts an ein idryer.org-Konto, eine TLS-MQTT-Sitzung mit automatischer Neuverbindung, Befehlsweiterleitung vom Portal und periodische Telemetrie-Veröffentlichung.

Sie schreiben nur das Gerätspezifische: Sensoren auslesen, Peripheriegeräte steuern. Alles andere ist in der Bibliothek.

## mqtt_contract.yaml — Single Source of Truth

Die Datei [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) definiert:

- **Fähigkeiten** — welche Peripheriegeräte jeder Gerätetyp unterstützt (Heizer, LED-Streifen, Sensoren);
- **Telemetrie-Felder** — Feldnamen und Datentypen in MQTT-Paketen;
- **UART-Protokoll** — Strukturen zwischen dem ESP32 und einem Co-Prozessor;
- **TypeScript-Typen** — für das Portal-Frontend.

Aus dieser Datei wird Code automatisch generiert:

| Was wird generiert | Wo |
|---|---|
| `iDryer::Config` (has* Flags) | `src/_generated/iDryer_api.h` |
| MQTT-Topics (C++ Konstanten) | `contracts/_generated/mqtt_topics.h` |
| TypeScript-Typen | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Bearbeiten Sie keine Dateien in `src/_generated/` und `contracts/_generated/` manuell — sie werden beim nächsten Regenerationslauf überschrieben.

## Wie man neue Peripheriegeräte hinzufügt

Das Verfahren ist für jede neue Fähigkeit gleich — eine Taste, ein CO2-Sensor, ein RFID-Leser.

**1.** Fügen Sie einen Eintrag zu `capability_vocabulary` in [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) hinzu:

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** Regeneration ausführen:

```bash
cd contracts
./regen.sh
```

Danach wird `iDryer::Config` ein `hasCo2` Feld haben, und TypeScript wird `HardwareUnitConfigCapabilities.co2` haben.

**3.** Setzen Sie das Flag in Ihrer `main.cpp`:

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Flashen Sie das Gerät. Das Portal liest `co2: true` aus dem MQTT `/info` Thema und zeigt den entsprechenden UI-Block automatisch an — keine Portal-Änderungen erforderlich.

Für Peripherietypen, die noch nicht im Vertrag stehen, öffnen Sie einen PR zum idryer-core-Repository mit einem Eintrag zu `capability_vocabulary`. Nach dem Merge — führen Sie `regen.sh` aus.

## Zwei Produktionsprodukte basierend auf dieser Bibliothek

**iDryer Storage Link** — ESP32-C3 mit einem WS2812B LED-Streifen und einem SHT31-Temperatur-/Feuchtigkeitssensor.

**iHeater Link** — ESP32-C3 mit RMT-Ausgang zum iHeater-Heizer mit Integrationen für Bambu Lab, Klipper/Moonraker und Home Assistant.

Beide Produkte enthalten idryer-core über PlatformIO `lib_deps` und implementieren nur ihre produktspezifische Logik.

## Nächste Schritte

- [01-wifi.md](01-wifi.md) — Verbinden Sie den ESP32 mit WiFi über Improv-Serial.
- [../../../README.md](../../../README.md) — Bibliotheksübersicht und Code-Generierungsreferenz.
