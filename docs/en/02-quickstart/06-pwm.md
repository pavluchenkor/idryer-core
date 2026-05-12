# Step 06 — Replacing RMT with PWM

After this step the same portal command flow will drive a PWM output instead of RMT. A typical use case is a heater controlled via a MOSFET or a DC dimmer.

## How it works

The executor is a plain callback function. `RmtOutputAdapter` from the previous step is one implementation. Replace it with `ledcWrite` code — everything else (MQTT, commands, status) stays unchanged.

## Steps

**1. Remove** the `RmtOutputAdapter` include and instance from `main.cpp`:

```cpp
// Remove:
#include "controller/RmtOutputAdapter.h"
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. Add PWM initialisation** in `setup()`:

```cpp
#define PWM_PIN     0      // GPIO for MOSFET gate
#define PWM_CHANNEL 0      // LEDC channel (0–15)
#define PWM_FREQ_HZ 25000  // 25 kHz — inaudible for most heaters
#define PWM_RES     8      // 8-bit → duty 0–255

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // off at startup
```

**3. In the command handler** replace `s_output.apply(cmd)` with `ledcWrite`:

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

**4. `loop()` does not change:**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` is the Arduino ESP32 API for versions before 3.x. In version 3.x and above use `ledcAttach(pin, freq, resolution)` and `ledcWrite(pin, duty)`. Check your version in `platformio.ini` (`platform = espressif32@X.Y.Z`).

## Verification

Press the **Heat** button on the portal. The output pin will carry a PWM signal with a duty cycle proportional to the `power` argument. Verify with a multimeter (average voltage) or an oscilloscope.

## What's next

- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — full `iDryer::Link` API reference.
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — template for any new actuator.
