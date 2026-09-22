# Ausführliche Einrichtung

Der kurze Weg ist [In 5 Minuten starten](01-five-minutes.md). Diese Seite behandelt Umgebung, Build-Flags, Logs und den Entwicklungsmodus.

## Der Kern im Projekt

Der Kern liegt in `lib/idryer-core` des PlatformIO-Projekts: als Kopie, Git-Submodul oder symbolischer Link auf einen gemeinsamen Klon. Seine `library.json` bringt die Abhängigkeiten mit: MQTT, ArduinoJson, WebSockets, Improv. In `lib_deps` des Projekts stehen nur die Bibliotheken Ihrer Sensoren.

Boards der Produkte auf dem Kern: ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP ist der ESP8266-Transport aus den Abhängigkeiten von espMqttClient: auf ESP32 nicht baubar.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Board ohne USB-UART (ESP32-C3 SuperMini): Serial über USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Flag | Wozu |
|---|---|
| `IDRYER_API_BASE` | Adresse der Portal-API: Aktivierung, Kopplung |
| `MQTT_BROKER`, `MQTT_PORT` | der Broker des Portals |
| `MQTT_USE_TLS=1` | sichere Verbindung zum Broker |
| `lib_ignore = ESPAsyncTCP` | ESP8266-Transport aus den Abhängigkeiten des MQTT-Clients: auf ESP32 nicht baubar |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial über USB bei Boards ohne USB-UART |

Zeichenketten-Makros brauchen Anführungszeichen außen und innen.

## Veröffentlichungsperioden

Die Felder `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs`; null bedeutet den Wert aus dem Vertrag:

| Was | Im Betrieb | Im Leerlauf |
|---|---|---|
| Telemetrie | 30 s | 60 s |
| Status | sofort bei Änderung von Modus, Sollwert oder Zeit; Abgleich alle 60 s | Abgleich alle 5 min |

„Leerlauf“ bedeutet: keine Einheit ist in einem aktiven Modus.

## Logs

```bash
pio device monitor -b 115200
```

Solange das Gerät kein Netz hat, schweigt das Log: Improv belegt den Port. Die Logs schalten sich mit der Zeile `[BOOT] WiFi ok, logs enabled` ein. Danach funktionieren die Befehle `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN`: siehe [Kopplung mit dem Konto](02-claim.md).

## Entwicklungsmodus: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Mit dem Flag:

- gehen Logs direkt nach dem Einschalten an den Port;
- sind Improv und die Befehle `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` aus: eingehende Zeilen des Ports liest Ihr Code;
- kommt das Netz aus der App (ESPTouch) oder aus dem Code: `seedWifiCredentialsIfEmpty()` vor `begin()`, siehe [WLAN](01-wifi.md).

Veröffentlichte Firmware wird ohne das Flag gebaut.

## Weiter

- [Beispiele des Kerns](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Ein neues Produkt hinzufügen](../09-add-product/01-add-new-product.md).
- [Gerätekarte: das Card-Manifest](../09-add-product/02-add-widget.md).
