# Actions without a mode: a relay and calling the device

After this page the card has the **Ventilate** form with a time in minutes and a device-call button in the header.

## What you need

- a relay module driven from 3.3 V (or a transistor switch) on pin 5;
- a fan or another load on the relay.

!!! warning
    A 230 V mains load needs isolation, a fuse and an enclosure. Without mains experience, build with a low-voltage load.

## Code

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // "fan on/off" cell
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 means the relay is off
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // an on/off change is published immediately
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // every 250 ms
    if (s_ventUntilMs && (int32_t)(millis() - s_ventUntilMs) >= 0) {
        s_ventUntilMs = 0;
        digitalWrite(RELAY_PIN, LOW);
        s_link.telemetry.fanOn[0] = false;
    }
    if (s_blinks) digitalWrite(LED_PIN, --s_blinks % 2);
}

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // No mode: the unit does not become busy, the form stays on the card.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Button that calls the device, in the card header.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## How it works

- An action without a mode (`mode` = `nullptr`) does not make the unit busy: the card shows no session block, the form stays in place.
- The `duration` purpose is a time in minutes: the field gets the **Time** label, the limits come from `.param(...)`.
- `hasFan` gives the "fan on/off" cell. The core publishes a `telemetry.fanOn` change immediately, without waiting for the period.
- `.deviceClass("identify")` is the permanent device-call button in the card header; `clear_errors` is error reset.

## Check

On the card set a time and tap **Ventilate**: the relay turns on and the fan cell shows "on". The call button in the header blinks the board LED.

## Next

[PWM heating](06-pwm.md).
