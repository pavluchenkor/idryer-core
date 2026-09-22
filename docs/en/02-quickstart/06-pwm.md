# PWM heating: mode, session, power

After this page the card starts heating with a temperature and a time, shows the session and the power, and the device holds the temperature with a PWM output driven by the sensor.

## What you need

- the SHT31 sensor from the [Telemetry step](03-telemetry.md);
- a logic-level MOSFET (opens at 3.3 V) on pin 3 and a heater for its supply voltage;
- in `platformio.ini`: `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    Do not leave a heater without a thermal fuse unattended: the firmware can hang and the MOSFET can fail shorted.

## Code

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // MOSFET gate
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // duty 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // heating power cell
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
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 means no limit
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Once a second: power from the gap to the setpoint, session time.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // No reading means the heater is off: never heat blind.
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

## How it works

- An action with the `HEATING` mode starts a session: the callback sets the mode, the setpoint and the duration and calls `publishStatusNow()`. By the mode the card shows the session block and **Stop**.
- The portal and the app label the `target_temperature` and `duration` purposes themselves. Numbers in `args` are already clamped to the parameter limits.
- `durationS = 0` means no time limit; the card shows `elapsedS` as elapsed time.
- `hasHeater` gives the power cell. While working, the core sends the average of `heaterPower01` over the last two publishing periods, so frequent PWM changes do not jump on the card.
- The regulator here is proportional: 10 °C below the setpoint means full power. For precise holding, replace it with a PID.
- `ledcSetup` / `ledcAttachPin` are the arduino-esp32 2.x API. In 3.x use `ledcAttach(pin, freq, bits)` and `ledcWrite(pin, duty)`.

## Check

Start **Heat** on the card: the session block appears with the setpoint and the time, and the power cell shows how much the heater is on. **Stop** turns the output off.

## Next

- [Device card: the card manifest](../09-add-product/02-add-widget.md): parameters from the device menu, profiles, card layout.
- [How to add a new product](../09-add-product/01-add-new-product.md).
