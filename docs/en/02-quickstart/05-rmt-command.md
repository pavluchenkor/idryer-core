# Step 05 — Portal commands: RMT output

After this step pressing the Start button on the portal will generate an RMT pulse on an ESP32 output pin. The example follows iHeater Link, where the pin drives the iHeater STM32 via an optocoupler.

## How it works

The portal publishes an `invoke` command to the MQTT topic `idryer/{serial}/commands/invoke`. The library deserialises the JSON and calls the registered handler. The handler passes the command to `RmtOutputAdapter`, which generates a pulse frame on the selected pin.

The handler is independent of the specific pin or protocol — it is a plain callback function. RMT is one implementation; PWM is another, see [06-pwm.md](06-pwm.md).

## What you need

- ESP32-C3 or ESP32 (RMT is available on all GPIO pins)
- A load on the output pin (in iHeater Link — an STM32 via an optocoupler)

## Steps

**1. Declare RmtOutputAdapter** in `main.cpp`. Based on [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp):

```cpp
#include "controller/RmtOutputAdapter.h"

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

The default output pin is `IHEATER_TRIGGER_OUTPUT_PIN`. Set it via `build_flags`:

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. Initialise** in `setup()`:

```cpp
s_output.begin();
```

`begin()` configures the RMT channel and starts a background FreeRTOS task that sends keepalive frames.

**3. Register the command handler** in `setup()`:

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

**4. In `loop()` — only call `device().loop()`:**

```cpp
void loop() {
    device().loop();
}
```

RMT frames are sent from the FreeRTOS task inside `s_output`, independently of `loop()`.

## How the portal sends a command

The portal publishes to the MQTT topic `idryer/{serial}/commands/invoke`:

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

The library receives this message and calls the registered callback with the deserialised `JsonObjectConst data`. The `action` field determines what to do.

The list of actions for each device type is defined in [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) under `invoke_actions`.

## Verification

Open the portal → device page → press the **Heat** button. In the Serial Monitor:

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

RMT pulses will appear on the output pin (verify with an oscilloscope or logic analyser).

## What's next

- [06-pwm.md](06-pwm.md) — replace RMT with PWM (MOSFET, DC dimmer).
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — RMT configuration: pulse frequency, Off code, temperature range.
