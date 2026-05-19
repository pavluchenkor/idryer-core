# idryer-core

[developer docs](https://dev.idryer.org/core/)

> **Before editing anything, read the "Code Generation" section below.**
> Some files in this repository are generated automatically, and manual changes will be overwritten.

---

Embedded library for ESP32 devices in the iDryer ecosystem.

If you are building a device that should work with the [iDryer portal](https://portal.idryer.org/) infrastructure (cloud, web portal, mobile app, printer integrations), this library provides the full integration layer: WiFi provisioning, claim flow, TLS MQTT session with auto-reconnect, command routing, and periodic telemetry publishing.
[App Store](https://apps.apple.com/app/idryer/id6760609044)
[Google Play](https://play.google.com/store/apps/details?id=org.idryer.mobile)


You only implement device-specific logic: sensor reads, peripheral control, and business logic. Everything else is handled by `iDryer::Link link(cfg); link.begin(); link.loop();`.

---

## Code Generation

**Single source of truth: [`contracts/mqtt_contract.yaml`](contracts/mqtt_contract.yaml)**

This file is used to generate:

| Generated artifact | Output path | Used by |
|---|---|---|
| `iDryer::Config` (`has*` flags) | `src/_generated/iDryer_api.h` | Firmware (`main.cpp`) |
| UART protocol (structs/enums/kind ids) | `contracts/_generated/uart_protocol.h` | UART bridge |
| MQTT topics (C++ constants) | `contracts/_generated/mqtt_topics.h` | Firmware |
| `HardwareUnitConfigCapabilities` | `contracts/_generated/mqtt-api.types.ts` | Portal (TypeScript) |

**Rule:** do not edit files in `src/_generated/` and `contracts/_generated/` manually. They are overwritten on the next regeneration.

### Run Regeneration

```bash
cd contracts
./regen.sh
```

Internally: YAML validation -> all generators in sequence. Usually takes around 1 second.

The pre-commit hook runs this automatically. Setup is described in [`contracts/HOOKS.md`](contracts/HOOKS.md).

### Add a New Capability

Example: add support for a button (`button`):

**1. Add to YAML:**

```yaml
# contracts/mqtt_contract.yaml → capability_vocabulary:
button:
  json_key: "button"
  config_flag: "hasButton"
  description: "Control button"
```

**2. Run regeneration:**

```bash
cd contracts && ./regen.sh
```

After that, `iDryer::Config` will include `hasButton`, and TypeScript will include `HardwareUnitConfigCapabilities.button`.

**3. In your device `main.cpp`:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasButton = true,   // field is now available
};
```

**4. Flash the device** — the portal reads `button: true` from `/info` and renders the corresponding UI block.

### Contract Navigation

```bash
cd contracts

# File map
python3 show.py

# Find a specific action
python3 show.py invoke_actions.storage_link.led.pulse

# All invoke actions across devices
python3 show.py --actions

# Device profiles (capability sets per device)
python3 show.py device_profiles
```

---

## Usage

Used in production devices:

- **iDryer Storage Link** - filament rack lighting control.
- **iHeater Link** - bridge between printer systems (Bambu/Klipper/HA) and an active iHeater-based thermal chamber.

Each device has its own product repository and uses this library via PlatformIO `lib_deps` or a symlink.

## Documentation

- Site: https://dev.idryer.org/core/
- In this repository: [`docs/ru/`](docs/ru/) - Russian docs.

5-minute quick start: [`docs/ru/02-quickstart/01-five-minutes.md`](docs/ru/02-quickstart/01-five-minutes.md).

Full public API reference: [`docs/ru/03-public-api/01-link-api-reference.md`](docs/ru/03-public-api/01-link-api-reference.md).

## License

[GPL-3.0](LICENSE). Any product using this library must publish its source code under a compatible license.

For questions not covered by the license, contact the author: [pavluchenkor](https://github.com/pavluchenkor).
