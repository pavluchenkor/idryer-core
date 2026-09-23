# Changelog

Notable changes to `idryer-core` — the shared SDK and protocol contract for the dryer, the heater and the storage cabinet. Russian version: [CHANGELOG.ru.md](CHANGELOG.ru.md).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## Types of changes

- **Added** — for new features.
- **Changed** — for changes in existing functionality.
- **Deprecated** — for soon-to-be removed features.
- **Removed** — for now removed features.
- **Fixed** — for any bug fixes.
- **Security** — in case of vulnerabilities.

## [3.0.0] — 2026-09-23

### Added

- **Firmware updates over the air.** A device updates itself, and the dryer also updates its iDryerControllerV2: the link module proxies the controller's firmware. After the reboot both versions are compared, and a mismatched pair rolls back.
- **New device binding.** No more PIN: the device receives its key on connection and is addressed in the cloud by its own identifier. The key can also be handed over on the local network.
- **Unbinding from the portal.** A portal command wipes the key, the device returns to the pairing state and no longer appears online in the cloud.
- **Local network control.** The mobile app talks to the device directly on the same network, without the cloud.
- **Separate command sources.** The "ignore external commands" setting closes the cloud but keeps local control.
- **Total run time.** The portal receives the accumulated run time instead of the time since the last power-up. Reboots and firmware updates no longer reset the counter.
- Telemetry: heater temperature, a separate stream for the scales, an empty value instead of a false zero when a sensor is unavailable.
- Heating mode for the heater and light animation mode for the light device.
- Type generation for Flutter — the mobile app is built from the same contract.
- **Access point chosen by signal strength.** The device scans the air and connects to the strongest access point of the network instead of the first one it sees. In a flat with a repeater or mesh this is the difference between a steady link and constant drops: on the bench the board picked the access point three metres away instead of a distant one and gained over 20 dB.
- **Roaming to a better access point at run time.** If the signal drops, or a noticeably stronger access point appears after a reconnect, the device moves by itself. Scans are rare and fade out: while the link holds steady there are none at all.
- **Publish periods come from the contract.** A product no longer needs to know them — a field left empty gets the value agreed with the portal. Separate idle periods were added: a device that does nothing reports less often.
- **Chamber temperature from printers with new firmware.** Bambu moved bed and chamber temperatures into a separate block, packing the current/target pair into a single number. The previous fields disappeared, and the owner of an updated printer got no chamber temperature at all. Both forms are read now.
- **Print progress and time left from the printer.** The device publishes the print percentage (Bambu and Moonraker) and the remaining time (Bambu — Klipper has no such field). The portal shows them on the heating card.
- **Chamber heating is released when the printer is gone.** If Klipper stops or disconnects, the chamber target is zeroed and a critical event goes to the portal and to the app. Previously the Moonraker link was considered healthy, no fresh data arrived, and heating continued blindly on the last known target. When the printer returns, a paired "control restored" message arrives in the event feed.
- **Integrations are compiled in by firmware flags.** `IDRYER_WITH_HA`, `IDRYER_WITH_BAMBU`, `IDRYER_WITH_MOONRAKER`: a disabled integration does not exist anywhere — not in the image, not in `integrations/status` — and a portal command for it is rejected. The image shrinks by 14–44 KB depending on what is left out.
- **The device declares its integrations.** The `supported` field in `integrations/status` lists what is compiled in; the portal and the app draw only those. Firmware without the field is shown as before.
- **Home Assistant is built from the card manifest.** Entities, values and commands come from the same declaration that draws the card in the portal. Hand-written entity lists in firmware are no longer needed.

### Changed

- **⚠️ Breaking change.** A zero publish period now means "take the value from the contract" instead of "do not publish". Products that do not need status have the new `Config.statusDisabled` flag. Firmware that used zero to switch status off will start publishing it after the update — it compiles silently, the behaviour changes. Replace the zero with the flag.
- **Major rework of the protocol contract.** A topic support matrix per product, one severity scale for events, one unit convention for temperatures over UART.
- Device capability flags follow the common contract vocabulary.
- **When the device offers to set up the network again.** The countdown starts from the loss of the link, not from power-up. Rebooting the router no longer brings up the setup prompt on a device with a valid password.
- **An access point that refuses the connection is excluded for a while** and the next strongest is taken. Previously the choice fell back to the system stack, which settled on a weak access point.
- The scaffold for third-party devices no longer suggests publish periods: they come from the contract.
- Heater power is sent to the portal averaged over time instead of as an instant sample: with rare publications the number stopped jumping.

### Removed

- PIN binding and the calls around it.
- `Config.allowHa`, `allowBambu`, `allowMoonraker` — the core never read them. What goes into the image is decided by the `IDRYER_WITH_*` flags.
- `HaPublisher`, `HaBuilder` and `Link::ha()` — Home Assistant entities are built by the generator from the manifest.

### Fixed

- The device no longer stays on a weak access point while a strong one is nearby. On the bench this lasted over four hours: the link held at −88 dBm with −62 available.
- Over-the-air setup mode is switched off after connecting. It used to keep running, hold the radio and disturb the link, and after the next drop it stopped starting until the device was rebooted.
- A failed start of setup mode is no longer repeated every cycle — it used to flood the log with tens of thousands of identical lines.
- Scanning the air no longer stalls the device: the screen and the controls stay alive.
- A failed scan no longer discards the access point already chosen.
- **Moonraker went silent after a Klipper restart.** Subscriptions to printer data live on the Klipper side, and Moonraker drops them when it loses the connection. The socket itself stays healthy, so the device did not notice the loss and waited for data until a reboot. The subscription is now restored as soon as the printer is ready again.
- The device no longer excludes the access point it is connecting to at that very moment. The failure counter counted seconds rather than refusals: a connection longer than eighteen seconds looked like three refusals in a row, and the best access point was dropped for five minutes. On the bench the device then spent a quarter of an hour hopping between access points at −86 dBm with −61 nearby.

## [2.x]

A series of trial builds. The direction was closed, the work carried over into 3.0.0.
