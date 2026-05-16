# Internals

Dieser Abschnitt ist für diejenigen, die über die Fassade hinausgegangen sind. Wenn `iDryer::Link` Ihre Anforderungen erfüllt — Sie brauchen nicht hierher zu kommen.

Es beschreibt die internen Bibliotheks-Komponenten: den Geräte-Koordinator, die UART-Transportschicht, Plattform-Abstraktionen und das Profil-Modell.

- [Runtime](01-runtime.md) — `IdryerRuntime`, Einstiegspunkt `begin()` / `loop()`
- [UART](02-uart.md) — Binäres Frame-Protokoll für Dual-MCU-Geräte
- [Integrationen](03-integrations.md) — Home Assistant, Bambu Lab, Moonraker/Klipper
- [Arduino-Plattform](04-platform-arduino.md) — WiFi, NVS, OTA-Schnittstellen
- [Profile](05-profiles.md) — `IProfile` Modell und Geräte-Verhalten
