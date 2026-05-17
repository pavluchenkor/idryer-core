# Interní implementace

Tato část je pro ty, kteří překročili fasádu. Pokud `iDryer::Link` pokrývá vaše potřeby — nemusíte sem chodit.

Popisuje interní součásti knihovny: koordinátor zařízení, vrstvu UART transport, abstrakce platformy a model profilu.

- [Runtime](01-runtime.md) — `IdryerRuntime`, vstupní bod `begin()` / `loop()`
- [UART](02-uart.md) — binární protokol rámce pro dual-MCU zařízení
- [Integrace](03-integrations.md) — Home Assistant, Bambu Lab, Moonraker/Klipper
- [Arduino platforma](04-platform-arduino.md) — WiFi, NVS, OTA rozhraní
- [Profily](05-profiles.md) — model `IProfile` a chování zařízení
