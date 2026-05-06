# What is idryer-core

If you are building an ESP32 device for the iDryer cloud, this library handles WiFi provisioning (Improv), the claim protocol, the MQTT session (TLS, reconnect, time sync), periodic telemetry/status publishing, and incoming command routing. Roughly 500 lines of boilerplate collapse into `link.begin(); link.loop();`.

## Minimal example

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## What the library does

- WiFi connection and keep-alive; Improv provisioning over Web Serial for initial setup.
- Claim protocol: device registration in the backend, account claiming via PIN.
- MQTT session with the iDryer broker: TLS, persistent session, auto-reconnect, NTP time sync.
- Periodic publishing of telemetry (`Telemetry`) and status (`Status`) on a timer.
- Routing of incoming commands (`commands/invoke`, `commands/set`, `commands/ping`) to the product handler.
- Local WebSocket server: a LAN client sees the same stream as the cloud.
- NVS persistence: WiFi credentials, device token, menu configuration across reboots.

## What the library does not do

- Does not manage product hardware: fans, heaters, LED strips, sensors.
- Does not contain business logic for drying, storage, or lighting.
- Does not know about product-specific menu parameters — it only transports them.
- Does not publish telemetry without data from the product: you fill `link.telemetry.*` yourself in `loop()`.

## Where to go next

- [Get started in 5 minutes](../02-quickstart/01-five-minutes.md)
- [Full API: iDryer::Link](../03-public-api/01-link-api-reference.md)
