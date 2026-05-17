# idryer-core — dokumentace knihovny

`idryer-core` — C++ knihovna (Arduino/PlatformIO) pro zařízení iDryer na bázi ESP32. Spravuje WiFi, MQTT, stavový automata v cloudu a směrování příkazů. Produkt implementuje pouze chování specifické pro zařízení.

Toto je dokumentace **knihovny**, nikoli konkrétního produktu.
Dokumentace produktu se nachází v [`docs/ru/`](../../docs/ru/).

---

## Rychlý start

**Tři věci, které implementujete:**

1. Implementujte `IProfile` — pět metod (config, info, loop).
2. Sestavte `main.cpp` — statické objekty, předejte závislosti přes konstruktory.
3. Zaregistrujte `handleCommand` — jeden handler pro MQTT a volitelně pro lokální WS.

**Tři věci, které knihovna dělá:**

1. Spravuje WiFi → zřizování → relaci MQTT.
2. Směruje příchozí příkazy na váš `handleCommand` (s výjimkou `ping`, který se zpracovává interně).
3. Volá vaše metody `IProfile` v pravý čas.

**Co můžete nechat nezměněné:**

- `ArduinoWifiManager`, `ArduinoCredentialStore` a další třídy `Arduino*` — používejte tak, jak jsou, bez podtříd.
- `CloudStateMachine` — vytvořte ji a předejte ji do `IdryerRuntime`; řídí se sama.
- `ActionDispatcher` — zpětná kompatibilita pro invoke/set; pro nový produkt jde zpracování příkazů přes `setCommandHandler()`, ne přes `ActionDispatcher`.

Praktický průvodce: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Pracovní příklady: [`examples/`](../../examples/)

---

## Oddíly

| Oddíl | Popis |
|-------|-------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Účel knihovny, co nedělá, kdo ji používá |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Tabulka všech modulů: účel, volitelnost |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Krátký úvod pro nového vývojáře: co zapojit, nahrát a co očekávat |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | Jak produkt sestavuje zásobník: pořadí vytváření objektů, pattern main.cpp |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | Co žije v knihovně, co v produktu |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Tok dat v běžícím zařízení: příchozí příkazy, odchozí zprávy, připojení |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | Třída `MqttClient`: konstruktor, připojení, publikování |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | Všechna MQTT témata: řetězce, zatížení, trvalá, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`: co koordinuje, které příkazy zpracovává |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | Most UART pro zařízení se dvěma MCU |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker: nastavení, omezení |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Implementace Arduino pro rozhraní zařízení |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | Rozhraní `IProfile`, zpětná volání, příklad `LedStripProfile` |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`: účel a pravidla pro úpravu |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Kontrolní seznam pro vytvoření nového produktu na základě `idryer-core` |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | Běžné problémy: WiFi, zřizování, MQTT, příkazy, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | Jak přidat senzor (zdroj dat) a publikovat jeho čtení |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | Jak přidat periférii a přijímat příkazy |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | Jak přidat paralelní přepravu (BLE, HTTP, vlastní) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Aplikované recepty pro předávání dat mezi senzory / periférie / profil / vydavatelé |
