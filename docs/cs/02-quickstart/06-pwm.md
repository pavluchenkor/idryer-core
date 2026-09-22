# Topení přes PWM: režim, relace, výkon

Po této stránce karta spouští topení s teplotou a časem, ukazuje relaci a výkon a zařízení drží teplotu výstupem PWM podle senzoru.

## Co budete potřebovat

- senzor SHT31 z [kroku Telemetrie](03-telemetry.md);
- MOSFET s logickou úrovní (otevírá se od 3,3 V) na pinu 3 a topení na jeho napájecí napětí;
- v `platformio.ini`: `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    Topení bez tepelné pojistky nenechávejte bez dozoru: firmware může zamrznout a MOSFET se může prorazit nakrátko.

## Kód

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // hradlo MOSFETu
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // střída 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // buňka výkonu topení
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
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 = bez omezení
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Jednou za sekundu: výkon podle rozdílu od žádané hodnoty, čas relace.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // Bez měření = topení vypnuto: nikdy netopit naslepo.
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

## Jak to funguje

- Akce s režimem `HEATING` spustí relaci: callback nastaví režim, žádanou hodnotu a dobu a zavolá `publishStatusNow()`. Podle režimu karta ukáže blok relace a **Stop**.
- Purposes `target_temperature` a `duration` popíšou portál i aplikace samy. Čísla v `args` jsou už omezena na meze parametru.
- `durationS = 0` znamená bez časového omezení; `elapsedS` karta ukazuje jako uplynulý čas.
- `hasHeater` dává buňku výkonu. Během práce jádro posílá průměr `heaterPower01` za poslední dvě periody publikace, takže časté změny PWM na kartě neskáčou.
- Regulátor je zde proporcionální: 10 °C pod žádanou hodnotou znamená plný výkon. Pro přesné držení ho nahraďte PID.
- `ledcSetup` / `ledcAttachPin` jsou API arduino-esp32 2.x. Ve 3.x použijte `ledcAttach(pin, freq, bits)` a `ledcWrite(pin, duty)`.

## Kontrola

Spusťte na kartě **Heat**: objeví se blok relace se žádanou hodnotou a časem a buňka výkonu ukáže, jak moc je topení zapnuté. **Stop** vypne výstup.

## Dál

- [Karta zařízení: card manifest](../09-add-product/02-add-widget.md): parametry z menu zařízení, profily, rozvržení karty.
- [Jak přidat nový produkt](../09-add-product/01-add-new-product.md).
