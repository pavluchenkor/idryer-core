# Internals

This section is for those who have gone beyond the facade. If `iDryer::Link` covers your needs — you do not need to come here.

It describes the internal library components: the device coordinator, the UART transport layer, platform abstractions, and the profile model.

- [Runtime](01-runtime.md) — `IdryerRuntime`, entry point `begin()` / `loop()`
- [UART](02-uart.md) — binary frame protocol for dual-MCU devices
- [Integrations](03-integrations.md) — Home Assistant, Bambu Lab, Moonraker/Klipper
- [Arduino platform](04-platform-arduino.md) — WiFi, NVS, OTA interfaces
- [Profiles](05-profiles.md) — `IProfile` model and device behaviour
