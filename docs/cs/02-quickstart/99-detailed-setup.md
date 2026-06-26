Pokud jste tu poprvé — jděte na [Začnete za 5 minut](01-five-minutes.md); tato stránka pokrývá pokročilé nastavení a řešení potíží.

Krátká cesta: zapojte knihovnu, flashujte příklad, vidíte blikající LED a zařízení na portálu.


- Deska ESP32 (doporučeno: ESP32-C3 DevKit, Super Mini, XIAO ESP32-S3, Waveshare ESP32-S3 Zero).
- PlatformIO s frameworkem `arduino`, platformou `espressif32`.
- WiFi 2,4 GHz s přístupem na internet.
- Účet na [portal.idryer.org](https://portal.idryer.org/) pro claim.


V `platformio.ini` vašeho produktu:

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; pouze potřebné pro mqtt_with_local_ws

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```


Zkopírujte [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) do `include/secrets.h` ve vašem projektu a vyplňte vaše SSID/heslo. Soubor musí být v `.gitignore`.

```cpp
```

`IDRYER_API_BASE` je obvykle nastaven přes `build_flags`, ne přes secrets.h.


Nejjednoduššího je [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino). Zkopírujte jej jako výchozí bod:

- Nevyžaduje žádné senzory, periférie nebo LAN WS.
- Nevyžaduje ruční `handleCommand` — vestavěný fallback v `IdryerRuntime` zvládá základní příkazy.
- LED bliká, když je zařízení online — to je indikátor úspěchu.


```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

Očekávaná posloupnost logu:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← čekání na claim
[CSM] PIN: 1234567   expires in 600s          ← pokud je auto-claim povolen
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← připraveno, LED začíná blikat
[RT]  Cloud Online
```


Automatické spárování je již v příkladu povoleno. PIN se objeví v logu. Zadejte jej na [portal.idryer.org](https://portal.idryer.org/) → "Přidat zařízení". Po spárování se `CloudStateMachine` přesune do stavu `Online`.


Následující příklady zavádějí vždy jednu novou úroveň složitosti:

| Příklad | Co je přidáno |
|---------|---------------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | vlastní `handleCommand`, zpracování `commands/invoke` a `commands/set` |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | WiFi provisioning přes Improv (bez hardcodovaných přihlašovacích údajů) |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | lokální LAN WebSocket server + `DevicePublisher` (jedna publikace — dva transporty) |


Alternativní cesta pro vývojáře — vidět celý tok claimu přímo v standardním Serial monitoru, bez Improv a bez portálu UI.

V `platformio.ini` vytvořte dev env s příznakem `-DIDRYER_DEV_REPL=1`:

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Co příznak povoluje:
- HAL logy na `Serial` se spustí **okamžitě** od startu (bez ticha až do připojení WiFi).
- Improv provisioning je **deaktivován** — Serial je volný pro interaktivní příkazy.
- Jednoduchý REPL se objeví v `main.cpp`: `wifi`, `claim`, `status`, `wipe`, `restart`, `help`.

Plný tok:

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

V monitoru:

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — enter in portal
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

REPL přijímá příkazy bez ohledu na nastavení konce řádku v Serial monitoru (`\n`, `\r`, nebo timeout nečinnosti 120 ms) — pracuje v jakémkoliv terminálu, včetně `pio device monitor`, Arduino IDE Serial Monitor, `screen`, `picocom`.

Produkční build (`-e my-device-prod`, bez `IDRYER_DEV_REPL`) používá Improv přes Chrome (`https://www.improv-wifi.com/`) a neobsahuje žádný REPL kód — příznak je compile-time, šetří Flash.

`secrets.h` s `WIFI_SSID/WIFI_PASSWORD` (Krok 2) zůstává oddělenou cestou pro bezheadless CI/auto-flash scénáře — funguje v obou prostředích.

Po spuštění jakéhokoliv z příkladů si přečtěte:

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — pořadí objektů v `main.cpp`.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — jak se data pohybují.
- [04-patterns/](../04-patterns/) — návody: přidejte senzor, periférii, transport.
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — plný checklist pro nový produkt.
- [10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — co dělat, když je stack zaseknutý.
