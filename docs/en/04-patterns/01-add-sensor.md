# Add a sensor

## When to use

If the device needs to periodically read a physical sensor (temperature, humidity, weight, etc.) and publish readings to the cloud or a LAN client — use this recipe.

## Ready-to-use code

Copy into your project and replace `MyClimate` with your class name:

```cpp
// MyClimate.h — product sensor driver
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // non-blocking, no delay()
    float temperature() const;
    float humidity()    const;
    bool  ok()          const;
};
```

```cpp
// main.cpp
#include <iDryer.h>
#include "MyClimate.h"

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link  s_link(CFG);
static MyClimate     s_climate;
static bool          s_sensorOk = false;

void setup() {
    s_sensorOk = s_climate.begin();
    s_link.begin();
}

void loop() {
    s_link.loop();

    if (s_sensorOk) {
        s_climate.tick(millis());
        if (s_climate.ok()) {
            s_link.telemetry.airTempC[0]       = s_climate.temperature();
            s_link.telemetry.airHumidityPct[0] = s_climate.humidity();
        }
    }
    // Publishing is automatic, on the telemetryPeriodMs timer from Config.
}
```

## Explanation

The product only fills the `s_link.telemetry.*` fields in `loop()`. The facade publishes them to MQTT and Local WS every `Config.telemetryPeriodMs` milliseconds on its own — no need to call `publishTelemetryNow()` manually. This is the key difference from manual MQTT: no `StaticJsonDocument`, no `publishTelemetry`, no separate publisher class.

If you need to publish readings immediately outside the timer — call `s_link.publishTelemetryNow()`.

The `hasAirTemp` / `hasAirHumidity` flags in `Config` control which fields appear in the JSON. A field whose flag is `false` is not published.

Full list of telemetry fields: [Telemetry fields](../03-public-api/01-link-api-reference.md#telemetry-fields).

## Full example in the repo

Reference implementation: `Sht31ClimateSensor` + filling `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]` in `iDryer-Storage/src/main.cpp`.
