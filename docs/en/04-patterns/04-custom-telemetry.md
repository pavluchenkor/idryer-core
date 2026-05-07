# Custom Telemetry (product-specific payload)

## When to use

idryer-core's standard telemetry publishes only the fields defined in the common contract (`units[].temperature`, `humidity`, `heaterPower`, etc.). If your product needs to add top-level JSON fields (e.g. `outputMode`, `targetTempC`, `active`) or include data not present in the `Telemetry` struct, use this recipe.

A typical case: iHeater Link publishes `outputMode` and `targetTempC` alongside the standard `units[]`, so the backend can forward `heaterIntent` to the frontend via the `telemetry:update` WebSocket event.

## Step 1 — Disable auto-publish

Set `telemetryPeriodMs = 0` in `Config`. This prevents idryer-core from publishing a stripped-down payload on its own:

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // publish manually
    .statusPeriodMs    = 5000,
};
```

## Step 2 — Write the publish function

Use `device().mqttClient()->publishTelemetry(doc)`. Include all fields the backend expects: both product-specific (top-level) and the standard `units[]` block.

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Current hardware output intent
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Active integration ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Product-specific top-level fields
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Standard units[] block — backend stores history from this
    // temperature/humidity = 0 if the device has no sensors
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // timestamp is added automatically
}
```

## Step 3 — Call from `loop()`

```cpp
void loop() {
    device().loop();

    static uint32_t s_lastTelMs = 0;
    if ((uint32_t)(millis() - s_lastTelMs) >= 5000u) {
        s_lastTelMs = millis();
        publishCustomTelemetry();
    }
    // ...
}
```

## What not to do

- **Do not publish both** idryer-core auto-telemetry (non-zero `telemetryPeriodMs`) and custom telemetry simultaneously. The backend receives two messages on the same topic and processes both — data gets duplicated.
- **Do not call `device().publishTelemetryNow()`** when `telemetryPeriodMs = 0` — it publishes the standard stripped payload without your product-specific fields.

## Why the library doesn't do this itself

idryer-core already publishes `heaterPower: 1` inside `units[]` — formally enough to know heating is active. The problem is not in the library but in the backend (`telemetry.handler.ts`): it looks specifically for a top-level `outputMode` field and does not derive `heaterIntent` from the standard `heaterPower`. This is technical debt on the backend side.

The current recipe is a temporary workaround. If the backend is fixed to derive `heaterIntent` from `units[0].heaterPower`, you can revert to `telemetryPeriodMs = 5000` and remove `publishCustomTelemetry()` — the standard library telemetry will work without any changes.

Watch for updates to `telemetry.handler.ts`: once a fallback on `heaterPower` is added there, this recipe becomes redundant.
