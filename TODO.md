# TODO

English is the main version. Russian: [TODO.ru.md](TODO.ru.md).

- **A Klipper macro that starts drying.**
  Keep it primitive: the macro holds a temperature (say 90 °C) and a time (240 minutes).
  The dryer reads the macro through Moonraker and starts drying. Nothing else is asked
  of it: no reply to the printer, no part in the print. The reading side is the same
  mechanism already built for iHeater's `VIRTUAL_CHAMBER` (`MoonrakerClient`), plus the
  link and touch firmware, which have no printer data handlers at all.

- **Start only the selected integration.**
  Objects of every compiled-in integration live in memory all the time. Create only the
  selected one at boot and skip the rest; switching takes a reboot. This gives back RAM
  wherever several integrations are compiled in — iHeater, for example.

- **A menu fingerprint in the handshake.**
  The menu has no version of its own: compatibility rests entirely on the controller and
  the link module sharing the same major version. If the menu changes and the major is
  not bumped, nothing catches the mismatch. The handshake should carry a fingerprint of
  the menu and compare it.

- **Status on connect.**
  While idle the core publishes status once every five minutes and does not publish it
  on connecting to the broker, so after a reboot the broker holds the mode of the
  previous run. Storage publishes it by itself; the core is not fixed.

- **Documentation on the old API.**
  The reference, the architecture pages, troubleshooting and `04-patterns/99-data-flow`
  describe the API from before the card manifest. Quickstart and the examples are done.
