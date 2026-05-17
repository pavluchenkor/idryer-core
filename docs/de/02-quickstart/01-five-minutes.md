# In 5 Minuten beginnen

Nach dieser Seite wird Ihr ESP32 geflasht, verbindet sich mit WiFi und erscheint in [portal.idryer.org](https://portal.idryer.org/) mit Status Online. Anforderungen: ESP32-C3 (DevKit, Super Mini oder kompatibel), USB-Kabel, PlatformIO in VS Code.

## 1. Bereiten Sie secrets.h vor

Kopieren Sie [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) zu `include/secrets.h` in Ihrem Projekt und setzen Sie Ihre WiFi-SSID und Ihr Passwort (nur 2,4 GHz):

```cpp
#define WIFI_SSID      "Ihre-SSID"
#define WIFI_PASSWORD  "Ihr-Passwort"
```

Fügen Sie `include/secrets.h` zu `.gitignore` hinzu.

## 2. Konfigurieren Sie platformio.ini

Erstellen Sie `platformio.ini` im Projektroot:

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://path/to/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

Ändern Sie `board` um Ihr Board zu entsprechen. Ersetzen Sie `path/to/idryer-core` durch den tatsächlichen Pfad zur Bibliothek.

## 3. Kopieren Sie das 01_blink_status Beispiel

Kopieren Sie den Inhalt von [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) in `src/main.cpp` Ihres Projekts. Das Beispiel erfordert keine Sensoren oder zusätzliche Abhängigkeiten — nur ein minimales Kompositionszentrum.

## 4. Flashen

```bash
pio run -e blink-demo -t upload
```

## 5. Öffnen Sie Serial Monitor

```bash
pio device monitor -b 115200
```

Erwartete Protokollsequenz:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

Nach Eingabe der PIN im Portal (Schritt 6):

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Wenn das Gerät bei der `PIN: ...` Nachricht gestoppt hat — das ist erwartet; fahren Sie mit Schritt 6 fort.

## 6. Beanspruchen Sie das Gerät im Portal

Öffnen Sie [portal.idryer.org](https://portal.idryer.org/), gehen Sie zu **Gerät hinzufügen** und geben Sie die PIN aus Serial Monitor ein. Nach erfolgreichem Anspruch wird das Gerät auf `Online` übergehen und die eingebaute LED blinkt alle 500 ms.

Detaillierter Claim-Fluss: [Onboarding](02-onboarding.md).

## Was ist als Nächstes zu tun

- Einen Sensor hinzufügen — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- Ein Peripheriegerät hinzufügen — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- Vollständige API-Referenz — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Wie es intern funktioniert — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
