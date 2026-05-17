# Schritt 05 — Portal-Befehle: RMT-Ausgang

Nach diesem Schritt erzeugt das Drücken der Start-Taste im Portal einen RMT-Impuls auf einem ESP32-Ausgabepin. Das Beispiel folgt iHeater Link, wo der Pin das iHeater STM32 über einen Optokoppler steuert.

## Wie es funktioniert

Das Portal veröffentlicht einen `invoke` Befehl zum MQTT-Thema `idryer/{serial}/commands/invoke`. Die Bibliothek deserialisiert das JSON und ruft den registrierten Handler auf. Der Handler übergibt den Befehl an `RmtOutputAdapter`, der einen Impuls-Frame auf dem ausgewählten Pin generiert.

Der Handler ist unabhängig vom spezifischen Pin oder Protokoll — es ist eine einfache Callback-Funktion. RMT ist eine Implementierung; PWM ist eine andere, siehe [06-pwm.md](06-pwm.md).

## Was Sie benötigen

- ESP32-C3 oder ESP32 (RMT ist auf allen GPIO-Pins verfügbar)
- Eine Last am Ausgabepin (in iHeater Link — ein STM32 über einen Optokoppler)

## Schritte

**1. Deklarieren Sie RmtOutputAdapter** in `main.cpp`. Basierend auf [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp):

```cpp
#include "controller/RmtOutputAdapter.h"

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

Der Standard-Ausgabepin ist `IHEATER_TRIGGER_OUTPUT_PIN`. Setzen Sie ihn über `build_flags`:

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. Initialisieren** in `setup()`:

```cpp
s_output.begin();
```

`begin()` konfiguriert den RMT-Kanal und startet eine Hintergrund-FreeRTOS-Aufgabe, die Keep-Alive-Frames sendet.

**3. Registrieren Sie den Befehls-Handler** in `setup()`:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action    = data["action"] | "";
    JsonObjectConst args  = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float    tempC  = args["tempC"]      | 0.0f;
        uint32_t durMin = args["durationMin"] | 0u;

        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = tempC;
        s_output.apply(cmd);

        device().status.mode[0]        = iDryer::UnitMode::Drying;
        device().status.targetTempC[0] = tempC;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::Off;
        cmd.targetTempC = 0.0f;
        s_output.apply(cmd);

        device().status.mode[0] = iDryer::UnitMode::Idle;
        device().publishStatusNow();
    }
});
```

**4. In `loop()` — rufen Sie nur `device().loop()` auf:**

```cpp
void loop() {
    device().loop();
}
```

RMT-Frames werden von der FreeRTOS-Aufgabe in `s_output` unabhängig von `loop()` gesendet.

## Wie das Portal einen Befehl sendet

Das Portal veröffentlicht zum MQTT-Thema `idryer/{serial}/commands/invoke`:

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

Die Bibliothek empfängt diese Nachricht und ruft den registrierten Callback mit den deserialisierten `JsonObjectConst data` auf. Das `action` Feld bestimmt, was zu tun ist.

Die Liste der Aktionen für jeden Gerätetyp wird in [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) unter `invoke_actions` definiert.

## Überprüfung

Öffnen Sie das Portal → Geräteseite → drücken Sie die **Heat** Taste. Im Serial Monitor:

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

RMT-Impulse erscheinen auf dem Ausgabepin (überprüfen Sie mit einem Oszilloskop oder Logic Analyzer).

## Nächste Schritte

- [06-pwm.md](06-pwm.md) — ersetzen Sie RMT durch PWM (MOSFET, DC Dimmer).
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — RMT-Konfiguration: Impulsfrequenz, Off-Code, Temperaturbereich.
