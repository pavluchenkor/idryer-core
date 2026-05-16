# Ein Widget und ein neues Gerät hinzufügen

Kompletter Zyklus: Von der Repo-Verzweigung bis zum zusammengeführten PR. Umfasst Firmware, Vertrag, React-Widget und Portal-Tests.

Wenn Sie nur Firmware ohne neues Widget brauchen — siehe [01-add-new-product.md](01-add-new-product.md).

---

## Voraussetzungen

- Python 3.9+ mit `pip install pyyaml jsonschema`
- Node.js 18+
- PlatformIO CLI
- Zugriff auf das iDryer Portal für UIKit-Tests

---

## Schritt 1. Fork und Clone

1. Forken Sie das `idryer-core` Repository auf GitHub.
2. Klonen Sie Ihren Fork lokal:

    ```bash
    git clone https://github.com/<your-username>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. Überprüfen Sie, dass der Vertrag die Validierung im aktuellen Zustand besteht:

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## Schritt 2. Bearbeiten Sie den Vertrag

Alle Änderungen gehen in `contracts/mqtt_contract.yaml`. Halten Sie alles in einem einzigen Changeset.

!!! warning
    Bearbeiten Sie keine Dateien in `_generated/` — sie werden von Generatoren überschrieben.

### 2a. Fähigkeits-Vokabular (neuer Peripherie-Typ)

Wenn das Gerät einen neuen Hardware-Typ hat (z.B. einen CO2-Sensor), fügen Sie einen Eintrag zum `capability_vocabulary` Abschnitt hinzu:

```yaml
capability_vocabulary:
  co2:
    description: "CO2 sensor (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

Dies fügt automatisch das Feld `hasAirCo2: bool` zu `iDryer::Config` bei der nächsten Regenerierung hinzu.

### 2b. Kanonische Rollen (neue Rolle + Widget)

Wenn das Gerät ein neues Menü-Element freigibt, registrieren Sie die Rolle in `canonical_roles`:

```yaml
canonical_roles:
  co2.read:
    type: float
    widget: Co2Display
    unit: ppm
    labels:
      ru: "CO₂"
      en: "CO₂"
```

Der `widget` Wert ist der Name der React-Komponente, die Sie in Schritt 5 schreiben.

### 2c. Invoke-Aktionen (falls das Widget Befehle sendet)

Wenn das Widget eine Aktion auf dem Gerät auslöst, beschreiben Sie es in `invoke_actions`:

```yaml
invoke_actions:
  my_device:
    co2.calibrate:
      description: "Start CO2 sensor calibration"
      args:
        targetPpm:
          type: uint16
          description: "Reference CO2 value (ppm)"
          required: true
```

### 2d. Geräteprofil (neuer Gerätetyp)

Fügen Sie das Profil zu `device_profiles` hinzu:

```yaml
device_profiles:
  my_device:
    description: "My device"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

Fähigkeitswerte stammen aus dem `capability_vocabulary` definiert in Schritt 2a.

---

## Schritt 3. Validieren und Regenerieren

```bash
cd contracts
./regen.sh
```

Flags:

| Flag | Effekt |
|---|---|
| (keine) | Validieren + alle Generatoren + zu Portal kopieren |
| `--firmware-only` | Nur Firmware-Generatoren, Portal-Kopie überspringen |
| `--help` | Hilfe anzeigen |

Bei Erfolg wird `_generated/` aktualisiert mit:

- `uart_protocol.h`, `mqtt_topics.h` — C++ Header
- `iDryer_api.h` — Config/DeviceType Fassade
- `mqtt-api.types.ts` — TypeScript-Typen
- `scaffolds/my_device/` — PlatformIO-Projektgerüst
- Im Portal: Dateien in `src/components/widgets/`

Wenn `regen.sh` mit einem Fehler beendet wird, beheben Sie das Problem, bevor Sie fortfahren.

---

## Schritt 4. Implementieren Sie Firmware

Verwenden Sie das generierte Gerüst-Projekt:

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

Füllen Sie die TODO-Abschnitte in `src/main.cpp`:

- `onOnline()` — Konfiguration aus NVS laden, Hardware initialisieren.
- `loop()` — Sensoren abfragen, `s_runtime.publishTelemetry(tel)` aufrufen.
- `buildInfoJson()` — Bereits vom Generator aus Fähigkeiten gefüllt.
- `onInvoke()` — `co2.calibrate` behandeln.
