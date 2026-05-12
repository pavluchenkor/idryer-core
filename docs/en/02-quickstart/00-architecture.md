# How idryer-core works

idryer-core is a library for ESP32 that handles the entire cloud stack: WiFi provisioning via Improv-Serial, the claim protocol for binding a device to an idryer.org account, a TLS MQTT session with auto-reconnect, command routing from the portal, and periodic telemetry publishing.

You write only what is specific to your device: reading sensors, driving peripherals. Everything else is inside the library.

## mqtt_contract.yaml — single source of truth

The file [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) defines:

- **capabilities** — what peripherals each device type supports (heater, LED strip, sensors);
- **telemetry fields** — field names and data types in MQTT packets;
- **UART protocol** — structures between the ESP32 and a co-processor;
- **TypeScript types** — for the portal frontend.

From this file, code is generated automatically:

| What is generated | Where |
|---|---|
| `iDryer::Config` (has* flags) | `src/_generated/iDryer_api.h` |
| MQTT topics (C++ constants) | `contracts/_generated/mqtt_topics.h` |
| TypeScript types | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Do not edit files in `src/_generated/` and `contracts/_generated/` manually — they are overwritten on the next regeneration run.

## How to add new peripherals

The procedure is the same for any new capability — a button, a CO2 sensor, an RFID reader.

**1.** Add an entry to `capability_vocabulary` in [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml):

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** Run regeneration:

```bash
cd contracts
./regen.sh
```

After this, `iDryer::Config` will have a `hasCo2` field, and TypeScript will have `HardwareUnitConfigCapabilities.co2`.

**3.** Set the flag in your device's `main.cpp`:

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Flash the device. The portal will read `co2: true` from the MQTT `/info` topic and display the corresponding UI block automatically — no portal-side changes needed.

For peripheral types not yet in the contract, open a PR to the idryer-core repository adding an entry to `capability_vocabulary`. After the merge — run `regen.sh`.

## Two production products built on this library

**iDryer Storage Link** — ESP32-C3 with a WS2812B LED strip and an SHT31 temperature/humidity sensor.

**iHeater Link** — ESP32-C3 with an RMT output to the iHeater heater, with integrations for Bambu Lab, Klipper/Moonraker, and Home Assistant.

Both products include idryer-core via PlatformIO `lib_deps` and implement only their product-specific logic.

## What's next

- [01-wifi.md](01-wifi.md) — connect the ESP32 to WiFi using Improv-Serial.
- [../../../README.md](../../../README.md) — library overview and code generation reference.
