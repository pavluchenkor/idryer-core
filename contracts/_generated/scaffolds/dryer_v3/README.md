# dryer_v3

Auto-generated scaffold. Capabilities: **heater, fan, weight, rfid**.

## Quick start

1. Copy `include/secrets.h.example` → `include/secrets.h`, fill WiFi credentials.
2. Open in VS Code with PlatformIO extension.
3. Fill `TODO` sections in `src/main.cpp` with your hardware logic.
4. Build and flash: `pio run -e dryer_v3-prod --target upload`
5. Claim the device on [portal.idryer.org](https://portal.idryer.org).

## Adding a new capability

1. Add entry to `capability_vocabulary` in `contracts/mqtt_contract.yaml`.
2. Run `cd contracts && bash regen.sh`.
3. Set the new `has*` flag in `CFG` inside `src/main.cpp`.
4. Flash the device — the portal picks up the new capability from `/info`.

## Business logic

Business logic (e.g. "if temp > 45 turn on fan") goes in the `loop()` method
of `DryerV3Profile`. The yaml only describes the interface
(what is published/accepted), not the device's internal behaviour.

If you want the threshold to be user-configurable from the portal, expose it as
a menu item and read it from NVS in `applyConfig()`.
