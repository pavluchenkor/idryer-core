# Schritt 06 — Ersetzen Sie RMT durch PWM

Nach diesem Schritt wird derselbe Portal-Befehlsfluss einen PWM-Ausgang statt RMT steuern. Ein typischer Anwendungsfall ist ein Heizer, der über einen MOSFET oder einen DC-Dimmer gesteuert wird.

## Wie es funktioniert

Der Executor ist eine einfache Callback-Funktion. `RmtOutputAdapter` aus dem vorherigen Schritt ist eine Implementierung. Ersetzen Sie ihn durch `ledcWrite` Code — alles andere (MQTT, Befehle, Status) bleibt unverändert.

## Schritte

**1. Entfernen** Sie die `RmtOutputAdapter` Include und Instanz aus `main.cpp`:

```cpp
// Entfernen:
#include "controller/RmtOutputAdapter.h"
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. Fügen Sie PWM-Initialisierung** in `setup()` hinzu:

```cpp
#define PWM_PIN     0      // GPIO für MOSFET Gate
#define PWM_CHANNEL 0      // LEDC Kanal (0–15)
#define PWM_FREQ_HZ 25000  // 25 kHz — unhörbar für die meisten Heizer
#define PWM_RES     8      // 8-Bit → Duty 0–255

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // aus beim Start
```

**3. Im Befehls-Handler** ersetzen Sie `s_output.apply(cmd)` durch `ledcWrite`:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action   = data["action"] | "";
    JsonObjectConst args = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float power01 = args["power"] | 1.0f;  // 0.0–1.0
        uint8_t duty  = (uint8_t)(power01 * 255.0f);
        ledcWrite(PWM_CHANNEL, duty);

        device().status.mode[0]             = iDryer::UnitMode::Drying;
        device().telemetry.heaterPower01[0] = power01;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        ledcWrite(PWM_CHANNEL, 0);

        device().status.mode[0]             = iDryer::UnitMode::Idle;
        device().telemetry.heaterPower01[0] = 0.0f;
        device().publishStatusNow();
    }
});
```

**4. `loop()` ändert sich nicht:**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` ist die Arduino ESP32 API für Versionen vor 3.x. In Version 3.x und höher verwenden Sie `ledcAttach(pin, freq, resolution)` und `ledcWrite(pin, duty)`. Überprüfen Sie Ihre Version in `platformio.ini` (`platform = espressif32@X.Y.Z`).

## Überprüfung

Drücken Sie die **Heat** Taste im Portal. Der Ausgabepin trägt ein PWM-Signal mit einem Duty-Zyklus proportional zum `power` Argument. Überprüfen Sie mit einem Multimeter (Durchschnittsspannung) oder einem Oszilloskop.

## Nächste Schritte

- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — vollständige `iDryer::Link` API-Referenz.
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — Vorlage für jeden neuen Aktuator.
