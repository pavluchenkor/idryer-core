# Add a peripheral

## When to use

If the device needs to control hardware on a command from the cloud or LAN — relay, heater, LED strip, motor — use this recipe.

## Ready-to-use code

```cpp
// main.cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::StorageLink,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "1.0.0",
};

static iDryer::Link s_link(CFG);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (!cmd) return;

    if (strcmp(cmd, "invoke") == 0) {
        const char* action = data["action"] | "";

        if (strcmp(action, "fan.on") == 0) {
            myFan.on();
            s_link.publishStatusNow();  // reflect new state immediately
            return;
        }
        if (strcmp(action, "fan.off") == 0) {
            myFan.off();
            s_link.publishStatusNow();
            return;
        }
    }

    if (strcmp(cmd, "drying") == 0) {
        float targetTempC  = data["targetTempC"]  | 45.0f;
        uint32_t durationS = data["durationS"]    | 0;
        myHeater.start(targetTempC, durationS);
        s_link.status.mode[0]        = iDryer::UnitMode::Drying;
        s_link.status.targetTempC[0] = targetTempC;
        s_link.status.durationS[0]   = durationS;
        s_link.publishStatusNow();
        return;
    }

    if (strcmp(cmd, "stop") == 0) {
        myHeater.stop();
        s_link.status.mode[0] = iDryer::UnitMode::Idle;
        s_link.publishStatusNow();
        return;
    }
}

void setup() {
    myFan.begin();
    myHeater.begin();
    s_link.begin();
    // IMPORTANT: setCommandHandler — strictly AFTER begin().
    // begin() installs its own dispatcher; our handleCommand must overwrite it.
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## Explanation

`s_link.runtime()->setCommandHandler(handleCommand)` is the single connection point for the command handler. After this call all incoming MQTT commands (`invoke`, `set`, `drying`, `stop`, `ping`, `get_config`, etc.) reach `handleCommand` directly.

`s_link.publishStatusNow()` — call after every change to `s_link.status.*`. This immediately sends the new state to the portal and LAN clients without waiting for the `statusPeriodMs` timer.

Never call `delay()` inside `handleCommand` — the call is synchronous from an MQTT callback; blocking it breaks the session. Place timers in the `loop()` of the product object.

### Alternative: `link.onRequest()`

For standard commands (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) a simpler callback via `onRequest()` is enough — no need to parse raw JSON:

```cpp
s_link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start:
            myHeater.start(r.targetTempC, r.durationS);
            break;
        case iDryer::RequestKind::Stop:
            myHeater.stop();
            break;
        default:
            break;
    }
});
```

`onRequest()` does not work alongside `setCommandHandler` — if the full handler is set, the `onRequest` callback is not called. See [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) for details.

## Full example in the repo

Reference implementation: `handleCommand` handling `drying` / `stop` in `iHeater-link/src/main.cpp`.
