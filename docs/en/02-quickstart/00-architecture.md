# How idryer-core works

idryer-core is a library for ESP32. It takes care of everything that connects a device to the portal and the app:

- Wi-Fi: the network comes from the iDryer app (ESPTouch) or from a web page over USB (Improv);
- linking to an account with a one-time token, and unlinking;
- a secure MQTT session with reconnection;
- local network access: the app controls the device even without internet;
- publishing telemetry and status, delivering commands;
- over-the-air firmware updates;
- the device card on the portal and in the app.

You write only your part: read sensors, drive loads, declare what the card shows and which operations it starts.

## One entry point: `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // your own device
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // temperature cell
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // your code
}
```

| You do | The core does |
|---|---|
| fill `s_link.telemetry.*` | publishes every 30 s, every 60 s when idle |
| change `s_link.status.*` (mode, setpoint, time) and call `publishStatusNow()` | delivers the status; the card switches by mode |
| declare `s_link.card()`: sensors, controls, actions | builds the card manifest and publishes it |
| register `s_link.onCommand(...)` | passes commands from the cloud and from the local network |

## Device card

The portal and the app draw the card from the card manifest that the device sends:

- the `Config.has*` flags give ready-made cells: temperature, humidity, heating power, fan and others;
- `card().sensor(...)` adds your own value by its path in telemetry;
- `card().action(...)` declares an operation: the unit mode after it and its start parameters.

No portal code is needed for this. Details: [Device card: the card manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml` is the source of truth

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) describes the protocol: topics, telemetry and status fields, device capabilities, the card manifest. It generates:

| What | Where |
|---|---|
| `iDryer::Config` (`has*` flags) and API structures | `src/_generated/iDryer_api.h` |
| MQTT topics | `contracts/_generated/mqtt_topics.h` |
| ESP32 ↔ controller UART protocol | `contracts/_generated/uart_protocol.h` |
| TypeScript types for the portal | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Do not edit files in `_generated/` by hand: `contracts/regen.sh` overwrites them from the contract.

Your own value needs no contract change: `card().sensor(...)` declares it. The contract changes when all products need a new capability: first `mqtt_contract.yaml`, then `regen.sh`, then the code.

## Products built on the core

- **iDryer Link**: the dryer's communication module, an ESP32 next to the controller, talking over UART.
- **iDryer Storage**: lighting for a spool rack, an addressable strip and an SHT31 sensor.
- **iHeater Link**: control of the iHeater heater, integrations with Bambu Lab, Klipper/Moonraker and Home Assistant.

## Next

[Run in 5 minutes](01-five-minutes.md).
