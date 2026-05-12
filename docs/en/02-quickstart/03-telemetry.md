# Step 03 — Telemetry: publish sensor data

After this step the ESP32 will read temperature and humidity from an SHT31 sensor and publish the values to the portal every 10 seconds. The portal will display them as a live graph.

## What you need

**Hardware:**

- SHT31 on an I2C breakout module (address 0x44 or 0x45)
- Wires: SDA, SCL, VCC (3.3 V), GND

**Software:**

- PlatformIO
- Library `robtillaart/SHT31 @ ^0.5.0`

## Steps

**1. Connect SHT31 to ESP32-C3** (default pins used by Storage Link):

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3.3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    Connect the sensor only with the board powered off.

**2. Add the library** to `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... other dependencies
```

**3. Include Wire and the sensor** in `main.cpp`. Based on [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp
#include <Wire.h>
#include "storage/sensors/Sht31ClimateSensor.h"

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. Initialise** in `setup()`:

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // auto-detects address 0x44 or 0x45
```

`begin()` returns `false` if no sensor is found. The device will continue running without it.

**5. Call `tick()` in `loop()` and update the telemetry fields:**

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airTempC[0]       = r.temperature;
        s_link.telemetry.airHumidityPct[0] = r.humidity;
    }
}
```

The library publishes all `telemetry.*` fields to MQTT automatically at the interval set by `telemetryPeriodMs` in `iDryer::Config`. The default is 10 000 ms.

**6. Enable the capability in `iDryer::Config`:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```

## Verification

Open the Serial Monitor. On successful sensor detection:

```
[MAIN] SHT31 at 0x44
```

On the portal, navigate to the device page — temperature and humidity readings update every 10 seconds.

If the sensor is not found a warning is logged and the device continues running. Check that address 0x44/0x45 is not occupied by another device on the bus.

## What's next

- [04-leds.md](04-leds.md) — visualise humidity with an LED strip colour.
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — sensor implementation.
