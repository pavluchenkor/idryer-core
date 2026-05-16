# Detaillierte Einrichtung

Wenn Sie zum ersten Mal hier sind — gehen Sie zu [In 5 Minuten beginnen](01-five-minutes.md); diese Seite behandelt erweiterte Einrichtung und Fehlerbehebung.

Kurzer Weg: verbinden Sie die Bibliothek, flashen Sie ein Beispiel, sehen Sie die blinkende LED und das Gerät im Portal.

## Was Sie vorbereiten

- ESP32 Board (empfohlen: ESP32-C3 DevKit, Super Mini, XIAO ESP32-S3, Waveshare ESP32-S3 Zero).
- PlatformIO mit Framework `arduino`, Plattform `espressif32`.
- WiFi 2,4 GHz mit Internetzugang.
- Konto auf [portal.idryer.org](https://portal.idryer.org/) zum Beanspruchen.

## Schritt 1. Verbinden Sie die Bibliothek

In Ihrer `platformio.ini`:

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; only needed for mqtt_with_local_ws

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

## Schritt 2. Erstellen Sie `secrets.h`

Kopieren Sie [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) zu `include/secrets.h` in Ihrem Projekt und füllen Sie Ihr SSID/Passwort aus. Die Datei muss in `.gitignore` sein.

```cpp
#define WIFI_SSID      "Ihre-SSID"
#define WIFI_PASSWORD  "Ihr-Passwort"
```

`IDRYER_API_BASE` wird normalerweise über `build_flags` gesetzt, nicht über secrets.h.

## Schritt 3. Öffnen Sie das erste Beispiel

Das einfachste ist [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino). Kopieren Sie es als Ausgangspunkt:

- Erfordert keine Sensoren, Peripheriegeräte oder LAN WS.
- Erfordert kein manuelles `handleCommand` — der integrierte Fallback in `IdryerRuntime` handhabt grundlegende Befehle.
- LED blinkt, wenn das Gerät online ist — das ist der Erfolgsindikator.

## Schritt 4. Flashen und beobachten

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

Erwartete Protokollsequenz:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← waiting for claim
[CSM] PIN: 1234567   expires in 600s          ← if auto-claim is enabled
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
```
