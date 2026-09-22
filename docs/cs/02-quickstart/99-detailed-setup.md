# Podrobné nastavení

Krátká cesta je [Spustit za 5 minut](01-five-minutes.md). Tato stránka popisuje prostředí, příznaky sestavení, logy a vývojový režim.

## Jádro v projektu

Jádro leží v `lib/idryer-core` projektu PlatformIO: jako kopie, git submodul nebo symbolický odkaz na společný klon. Jeho `library.json` přinese závislosti: MQTT, ArduinoJson, WebSockets, Improv. V `lib_deps` projektu jsou jen knihovny vašich senzorů.

Desky produktů na jádře: ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP je transport ESP8266 ze závislostí espMqttClient: na ESP32 se nesestaví.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Deska bez USB-UART (ESP32-C3 SuperMini): Serial přes USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Příznak | K čemu |
|---|---|
| `IDRYER_API_BASE` | adresa API portálu: aktivace, spárování |
| `MQTT_BROKER`, `MQTT_PORT` | broker portálu |
| `MQTT_USE_TLS=1` | zabezpečené spojení s brokerem |
| `lib_ignore = ESPAsyncTCP` | transport ESP8266 ze závislostí klienta MQTT: na ESP32 se nesestaví |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial přes USB u desek bez USB-UART |

Řetězcová makra potřebují uvozovky vně i uvnitř.

## Periody publikace

Pole `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs`; nula znamená hodnotu z kontraktu:

| Co | V práci | V klidu |
|---|---|---|
| telemetrie | 30 s | 60 s |
| stav | hned při změně režimu, žádané hodnoty nebo času; sladění každých 60 s | sladění každých 5 min |

„V klidu“ znamená, že žádná jednotka není v aktivním režimu.

## Logy

```bash
pio device monitor -b 115200
```

Dokud zařízení nemá síť, log mlčí: port drží Improv. Logy se zapnou řádkem `[BOOT] WiFi ok, logs enabled`. Potom fungují příkazy `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN`: viz [Spárování s účtem](02-claim.md).

## Vývojový režim: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

S příznakem:

- logy jdou do portu hned po zapnutí;
- Improv a příkazy `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` jsou vypnuté: příchozí řádky portu čte váš kód;
- síť předá aplikace (ESPTouch) nebo kód: `seedWifiCredentialsIfEmpty()` před `begin()`, viz [Wi-Fi](01-wifi.md).

Vydávaný firmware se sestavuje bez příznaku.

## Dál

- [Příklady jádra](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Jak přidat nový produkt](../09-add-product/01-add-new-product.md).
- [Karta zařízení: card manifest](../09-add-product/02-add-widget.md).
