# idryer-core — library documentation

`idryer-core` — C++ library (Arduino/PlatformIO) for ESP32-based iDryer devices. Manages WiFi, MQTT, the cloud state machine, and command routing. The product implements only device-specific behaviour.

This is documentation for the **library**, not for any specific product.
Product documentation is located in [`docs/ru/`](../../docs/ru/).

---

## Quick start

**Three things you implement:**

1. Implement `IProfile` — five methods (config, info, loop).
2. Assemble `main.cpp` — static objects, pass dependencies through constructors.
3. Register `handleCommand` — a single handler for MQTT and optionally for the local WS.

**Three things the library does:**

1. Manages WiFi → provisioning → the MQTT session.
2. Routes incoming commands to your `handleCommand` (except `ping`, which is handled internally).
3. Calls your `IProfile` methods at the right moments.

**What you can leave untouched:**

- `ArduinoWifiManager`, `ArduinoCredentialStore`, and other `Arduino*` classes — use as-is, no subclassing needed.
- `CloudStateMachine` — create it and pass it to `IdryerRuntime`; it manages itself from there.
- `ActionDispatcher` — compatibility fallback for invoke/set; for a new product, command handling goes through `setCommandHandler()`, not through `ActionDispatcher`.

Practical guide: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Working examples: [`examples/`](../../examples/)

---

## Sections

| Section | Description |
|---------|-------------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Library purpose, what it does not do, who uses it |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Table of all modules: purpose, optionality |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Short onramp for a new developer: what to wire up, flash, and expect |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | How the product assembles the stack: object creation order, main.cpp pattern |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | What lives in the library, what lives in the product |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Data flow in a running device: incoming commands, outgoing messages, connections |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | `MqttClient` class: constructor, connection, publishing |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | All MQTT topics: strings, payloads, retained, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`: what it coordinates, which commands it handles |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | UART bridge for dual-MCU devices |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker: setup, limitations |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Arduino implementations of device interfaces |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | `IProfile` interface, callbacks, `LedStripProfile` example |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`: purpose and rules for modification |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Checklist for building a new product on top of `idryer-core` |
| [10-troubleshooting](10-troubleshooting.md) | Common issues: WiFi, provisioning, MQTT, commands, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | How to add a sensor (data source) and publish its readings |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | How to add a peripheral and receive commands |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | How to add a parallel transport (BLE, HTTP, custom) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Applied recipes for passing data between sensors / peripherals / profile / publishers |
