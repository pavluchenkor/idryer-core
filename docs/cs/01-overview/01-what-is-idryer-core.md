# Co je idryer-core

Pokud vytváříte zařízení ESP32 pro cloud iDryer, tato knihovna zpracovává zřizování WiFi (Improv), protokol claim, relaci MQTT (TLS, reconnect, time sync), periodické publikování telemetrie/stavu a směrování příchozích příkazů. Přibližně 500 řádků boilerplate se zhroutí do `link.begin(); link.loop();`.

## Minimální příklad

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

## Co knihovna dělá

- Připojení WiFi a keep-alive; Improv zřizování přes Web Serial pro počáteční nastavení.
- Protokol claim: registrace zařízení v backendu, claim účtu přes PIN.
- Relace MQTT s brokerem iDryer: TLS, trvalá relace, auto-reconnect, NTP time sync.
- Periodické publikování telemetrie (`Telemetry`) a stavu (`Status`) na časovač.
- Směrování příchozích příkazů (`commands/invoke`, `commands/set`, `commands/ping`) na handler produktu.
- Lokální WebSocket server: LAN klient vidí stejný stream jako cloud.
- NVS persistence: přihlašovací údaje WiFi, token zařízení, konfigurace menu v reboots.

## Co knihovna nedělá

- Nespravuje hardware produktu: ventilátory, topidla, LED pásky, senzory.
- Neobsahuje obchodní logiku sušení, skladování nebo osvětlení.
- Neví o parametrech menu specifických pro produkt — pouze je transportuje.
- Nepublikuje telemetrii bez dat z produktu: sami vyplníte `link.telemetry.*` v `loop()`.

## Kam dál

- [Začněte za 5 minut](../02-quickstart/01-five-minutes.md)
- [Úplné API: iDryer::Link](../03-public-api/01-link-api-reference.md)
