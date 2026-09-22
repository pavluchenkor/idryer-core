<div align="center">

# idryer-core

**A library for ESP32. Two lines of code connect your device to the iDryer ecosystem.**

[![Docs](https://img.shields.io/badge/docs-idryer.org-e7352c)](https://docs.idryer.org/en/development/core/) [![Telegram](https://img.shields.io/badge/Telegram-iDryer-2ca5e0)](https://t.me/iDryer) [![Discord](https://img.shields.io/badge/Discord-join-5865f2)](https://discord.gg/jGce5eeHHz) [![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)

</div>

---

## What this is

The shared integration layer for iDryer devices. If your device has to work with the [portal](https://portal.idryer.org/), the mobile app and printer integrations, this library covers everything between the hardware and the cloud.

About five hundred lines of boilerplate collapse into two calls:

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

That is a working device: it connects to Wi-Fi, binds to an account and shows up in the portal with its telemetry.

## Who it is for

For 3D printing enthusiasts who invent their own devices and want a result fast, not an infrastructure project.

- **You built a device and want to control it remotely.** From your phone, from a browser, with a push notification when something happens. The portal, the mobile app, account binding, telemetry and charts already exist. One evening, and your device appears in the app with its own card.
- **You write only your part.** Sensors, actuators, logic. Wi-Fi, secure transport, commands and over-the-air updates come with the library.
- **You extend the ecosystem.** Firmware, portal and the bridge between microcontrollers all read one contract. This is where it lives.

## What it handles

- **Wi-Fi** connection, keep-alive, and first-time setup over Improv on Web Serial.
- **Binding** registration in the backend and linking to a user account by PIN.
- **MQTT** broker session: TLS, persistent session, automatic reconnect, time sync.
- **Telemetry and status** published periodically on a timer.
- **Commands** routing of incoming `invoke`, `set` and `ping` into your product handler.
- **Local WebSocket** a client on the same network sees the same stream as the cloud.
- **Storage** Wi-Fi credentials, device token and menu configuration survive a reboot.
- **Printer integrations** Home Assistant, Bambu Lab, Moonraker: the device learns the print state with no code on your side.
- **Over-the-air updates** receiving firmware for the ESP32 and proxying it to a second microcontroller over UART.

## What it does not do

The library does not touch hardware and does not know what device you are building. Fans, heaters, LED strips and sensors are yours. So is the drying, storage or lighting logic.

Telemetry does not appear on its own: you fill in `link.telemetry.*` in your own `loop()`.

The boundary is deliberate. The library owns transport, your product owns meaning.

## The contract is the source of truth

The protocol is described in a single file: [`contracts/mqtt_contract.yaml`](contracts/mqtt_contract.yaml). Everything else is generated from it:

| What is generated | Where | For whom |
|---|---|---|
| `iDryer::Config` with `has*` flags | `src/_generated/iDryer_api.h` | firmware |
| UART protocol: structs, enums, codes | `contracts/_generated/uart_protocol.h` | bridge between microcontrollers |
| MQTT topics as constants | `contracts/_generated/mqtt_topics.h` | firmware |
| Capability types | `contracts/_generated/mqtt-api.types.ts` | portal |

Firmware, bridge and portal cannot drift apart, because they share one description.

> **Never edit files in `_generated/` by hand.** The next generation run overwrites them. Edit the YAML, then run `cd contracts && ./regen.sh`, about a second. The pre-commit hook does it for you.

## Two ways to get an interface

**Your own device: the card builds itself.** The firmware declares a list of entities: sensors, numeric fields, switches, buttons. The portal receives that description and renders the card from it. No changes on the portal side are needed. This mechanism exists precisely for community devices.

**Ecosystem products: cards are written by hand.** The dryers, iHeater and Storage have their own interfaces, polished for a specific product, and they are not built from a description.

An important consequence. Adding a new capability to the shared contract vocabulary, `hasButton` for example, is not enough to make it appear in the interface of product devices: the vocabulary describes the protocol, and the product card has to be extended separately. If you need a capability in the shared vocabulary, start with an issue. It has to be agreed on.

For your own device nothing needs to be agreed on: declare the entities in the firmware and get a card.

→ [Add your own product](https://docs.idryer.org/en/development/core/09-add-product/01-add-new-product/)

## Where it is used

The library is the foundation of every device in the ecosystem: [iDryer Link](https://github.com/pavluchenkor/iDryer-Link), [iHeater Link](https://github.com/pavluchenkor/iHeater-Link), [iDryer Storage](https://github.com/pavluchenkor/iDryer-Storage), [iDryer Touch](https://github.com/pavluchenkor/idryer-touch), [iDryer Controller V2](https://github.com/pavluchenkor/iDryerControllerV2).

Add it through `lib_deps` in PlatformIO or with a symlink.

If you want to build your own device on it, there are complete end-to-end examples with every step explained: [Build Your Own iDryer](https://docs.idryer.org/en/development/byod/).

## Where to start

**[Get running in five minutes](https://docs.idryer.org/en/development/core/02-quickstart/01-five-minutes/)** from an empty folder to a device showing Online in the portal. You need an ESP32-C3, a cable and PlatformIO.

Then, as needed:

- [What idryer-core is and when you need it](https://docs.idryer.org/en/development/core/01-overview/01-what-is-idryer-core/)
- [Full API reference](https://docs.idryer.org/en/development/core/03-public-api/01-link-api-reference/)
- [Add a sensor](https://docs.idryer.org/en/development/core/04-patterns/01-add-sensor/)
- [How the contract works](https://docs.idryer.org/en/development/core/08-contracts/01-mqtt-contract/)
- [What to do when it does not work](https://docs.idryer.org/en/development/core/10-troubleshooting/01-troubleshooting/)

## Status

The library is the foundation of every device in the ecosystem. The protocol contract is shared by firmware and portal, and changes go through generation.

## What is in the repository

| Path | What it is |
|---|---|
| `src/` | The library |
| `contracts/` | Protocol contract, generators, YAML navigation |
| `examples/` | Ready-to-build examples, from minimal to complete |
| `menu/` | The menu described as a protocol |

## License

Code: [Apache License 2.0](LICENSE), [NOTICE](NOTICE).

The iDryer name is not covered by the license, see [TRADEMARKS.md](TRADEMARKS.md).

Releases up to and including the last GPL-3.0 tag remain available under GPL-3.0. Apache-2.0 applies from the first release that contains the current LICENSE file.

## Help

- [Telegram](https://t.me/iDryer)
- [Discord](https://discord.gg/jGce5eeHHz)
- [Documentation](https://docs.idryer.org/en/development/core/)

## Contributing

Built a device on the core, found a bug, missed a capability in the contract? Open an issue or send a pull request.

Before making changes, read the section on generation: some files in the repository are created automatically.

## Next

[Five minutes to your first device](https://docs.idryer.org/en/development/core/02-quickstart/01-five-minutes/): flash an ESP32 and see it in the portal.
