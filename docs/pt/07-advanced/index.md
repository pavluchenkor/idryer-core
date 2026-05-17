# Internals

Esta seção é para aqueles que foram além da fachada. Se `iDryer::Link` atender às suas necessidades — você não precisa vir aqui.

Descreve os componentes internos da biblioteca: o coordenador de dispositivos, a camada de transporte UART, abstrações de plataforma e o modelo de perfil.

- [Runtime](01-runtime.md) — `IdryerRuntime`, ponto de entrada `begin()` / `loop()`
- [UART](02-uart.md) — protocolo de quadro binário para dispositivos com dois MCUs
- [Integrações](03-integrations.md) — Home Assistant, Bambu Lab, Moonraker/Klipper
- [Plataforma Arduino](04-platform-arduino.md) — interfaces WiFi, NVS, OTA
- [Perfis](05-profiles.md) — modelo `IProfile` e comportamento do dispositivo
