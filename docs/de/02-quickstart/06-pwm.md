# Heizen per PWM: Modus, Sitzung, Leistung

Nach dieser Seite startet die Karte das Heizen mit Temperatur und Zeit, zeigt Sitzung und Leistung, und das Gerät hält die Temperatur mit einem PWM-Ausgang nach dem Sensor.

## Was Sie brauchen

- den SHT31-Sensor aus dem [Schritt „Telemetrie“](03-telemetry.md);
- einen Logic-Level-MOSFET (öffnet bei 3,3 V) an Pin 3 und einen Heizer für dessen Versorgungsspannung;
- in `platformio.ini`: `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    Einen Heizer ohne Thermosicherung nicht unbeaufsichtigt lassen: die Firmware kann hängen, der MOSFET kann durchlegieren.

## Code

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // MOSFET-Gate
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // Tastgrad 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // Zelle der Heizleistung
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "PWM Heater",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);

static void readSensor() {
    const bool ok = s_sht.read();
    s_link.telemetry.airTempC[0]       = ok ? s_sht.getTemperature() : NAN;
    s_link.telemetry.airHumidityPct[0] = ok ? s_sht.getHumidity()    : NAN;
}

static uint32_t s_startMs = 0;

static void stopHeat(uint8_t unit) {
    ledcWrite(PWM_CHANNEL, 0);
    s_link.telemetry.heaterPower01[unit] = 0.0f;
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 = ohne Begrenzung
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Einmal pro Sekunde: Leistung aus dem Abstand zum Sollwert, Sitzungszeit.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // Kein Messwert = Heizung aus: niemals blind heizen.
    float power = 0.0f;
    if (!isnan(t)) power = constrain((s_link.status.targetTempC[0] - t) * 0.1f, 0.0f, 1.0f);
    ledcWrite(PWM_CHANNEL, (uint32_t)(power * 255));
    s_link.telemetry.heaterPower01[0] = power;

    const uint32_t elapsed = (millis() - s_startMs) / 1000;
    s_link.status.elapsedS[0] = elapsed;
    if (s_link.status.durationS[0] && elapsed >= s_link.status.durationS[0]) stopHeat(0);
}

void setup() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(8, 9);
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");

    s_link.every(2000, readSensor);
    s_link.every(1000, regulate);
}

void loop() {
    s_link.loop();
}
```

## So funktioniert es

- Eine Aktion mit dem Modus `HEATING` startet eine Sitzung: der Callback setzt Modus, Sollwert und Dauer und ruft `publishStatusNow()`. Nach dem Modus zeigt die Karte den Sitzungsblock und **Stop**.
- Die Purposes `target_temperature` und `duration` beschriften Portal und App selbst. Zahlen in `args` sind bereits auf die Grenzen des Parameters begrenzt.
- `durationS = 0` bedeutet ohne Zeitgrenze; `elapsedS` zeigt die Karte als vergangene Zeit.
- `hasHeater` liefert die Leistungszelle. Im Betrieb sendet der Kern den Mittelwert von `heaterPower01` über die letzten zwei Veröffentlichungsperioden, daher springen häufige PWM-Änderungen auf der Karte nicht.
- Der Regler hier ist proportional: 10 °C unter dem Sollwert bedeutet volle Leistung. Für genaues Halten durch einen PID ersetzen.
- `ledcSetup` / `ledcAttachPin` sind die API von arduino-esp32 2.x. In 3.x `ledcAttach(pin, freq, bits)` und `ledcWrite(pin, duty)` verwenden.

## Prüfen

Starten Sie **Heat** auf der Karte: der Sitzungsblock erscheint mit Sollwert und Zeit, die Leistungszelle zeigt, wie viel der Heizer eingeschaltet ist. **Stop** schaltet den Ausgang ab.

## Weiter

- [Gerätekarte: das Card-Manifest](../09-add-product/02-add-widget.md): Parameter aus dem Gerätemenü, Profile, Kartenlayout.
- [Ein neues Produkt hinzufügen](../09-add-product/01-add-new-product.md).
