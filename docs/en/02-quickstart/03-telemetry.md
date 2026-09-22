# Telemetry: a sensor on the card

After this page the device reads temperature and humidity from an SHT31, and the card shows their cells and your own value, the dew point.

## What you need

- an SHT31 module (I2C, address 0x44 or 0x45);
- wires: SDA, SCL, 3.3 V, GND;
- in `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Connect the sensor with the board powered off.

## Code

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // temperature cell
    .hasAirHumidity  = true,    // humidity cell
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // address 0x44 or 0x45, set by the module jumper

// Dew point by the Magnus formula: an example of your own value.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // No data is NAN: the field is not sent, the card shows "—".
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL: pins of your board
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // read every 2 s
}

void loop() {
    s_link.loop();
}
```

## How it works

- `hasAirTemp` and `hasAirHumidity` in `Config` give temperature and humidity cells on the card. No portal code is needed.
- The core publishes the `s_link.telemetry.*` fields itself: every 30 s while at least one unit is working, otherwise every 60 s. The `Config.telemetryPeriodMs` and `telemetryPeriodIdleMs` fields change the periods; zero means the contract value.
- `NAN` means no data: the field is not published. Do not put zero instead, or the chart shows a false drop.
- Your own value: `onTelemetryPublish` adds a field to telemetry before publishing, `card().sensor(id, label, unit, path, deviceClass)` declares it on the card. `path` is the path in telemetry; `deviceClass` is optional and sets the icon and format.
- `s_link.every(ms, fn)` calls a function from `loop()` with the given period without blocking the connection.

## Check

Within a minute after flashing, the card shows temperature, humidity and dew point. Without the sensor the cells are empty and the device keeps working.

## Next

[LED strip](04-leds.md).
