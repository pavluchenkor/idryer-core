# dryer_v3

Auto-generated scaffold on `iDryer::Link`. Capabilities: **heater, fan, weight, rfid, air_temp, air_humidity, heater_temp, servo**.

## Quick start

1. Copy this directory and put idryer-core into `lib/idryer-core`
   (a copy, a git submodule or a symbolic link).
2. Fill the `TODO` sections in `src/main.cpp` with your hardware logic.
3. Build and flash: `pio run -e dryer_v3-prod -t upload`.
4. Wi-Fi and pairing: in the iDryer app tap **Connect a new device**,
   pass the network on the **Wi-Fi** step and pair on the **Pairing** step.

The firmware has no network password and no account data: the core
gets both from the app.

## Next

- Quick start: `docs/en/02-quickstart/`.
- Device card, actions and parameters: `docs/en/09-add-product/02-add-widget.md`.
