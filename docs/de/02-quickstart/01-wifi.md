# Schritt 01 — WiFi-Bereitstellung mit Improv

Nach diesem Schritt wird Ihr ESP32 mit WiFi verbunden und die Anmeldedaten werden im NVS gespeichert, um beim nächsten Neustart automatisch eine Verbindung herzustellen. Portal und MQTT folgen im nächsten Schritt.

## Was Sie benötigen

**Hardware:**

- ESP32-C3 Board (DevKit, Super Mini oder kompatibel)
- USB-Kabel (USB-C oder Micro-USB je nach Ihrem Board)

**Software:**

- PlatformIO in VS Code
- Chrome oder Edge Browser (Web Serial API wird in Safari oder Firefox nicht unterstützt)

## Schritte

**1. Erstellen Sie `platformio.ini`** im Root Ihres Projekts:

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Ersetzen Sie `board` durch den Wert für Ihr Board (`esp32-c3-devkitm-1`, `seeed_xiao_esp32c3`, usw.).

**2. Kopieren Sie das Beispiel.** Nehmen Sie den Inhalt von [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) und speichern Sie ihn als `src/main.cpp` in Ihrem Projekt.

**3. Setzen Sie die ChipFamily.** In der kopierten Datei finden Sie die Zeile:

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

Stellen Sie sicher, dass die ChipFamily Ihrem Chip entspricht: `CF_ESP32_C3`, `CF_ESP32_S3` oder `CF_ESP32`.

**4. Flashen:**

```bash
pio run -e improv-demo -t upload
```

**5. Öffnen Sie [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)** in Chrome oder Edge. Klicken Sie auf **Verbinden** und wählen Sie den USB-Port des Geräts aus dem Browser-Dialog.

**6. Geben Sie die SSID und das Passwort** für Ihr 2,4-GHz-Netzwerk ein. Die Webseite sendet die Anmeldedaten über Serial-Improv an das Board. Das Board speichert sie im NVS.

## Überprüfung

Öffnen Sie Serial Monitor:

```bash
pio device monitor -b 115200
```

Nach erfolgreichem Verbindungsaufbau sehen Sie:

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

Wenn diese Zeile nicht angezeigt wird, siehe Link zur Fehlerbehebung unten.

!!! note
    Wenn Anmeldedaten bereits im NVS aus einem vorherigen Lauf gespeichert sind, verbindet sich das Board beim Booten automatisch mit WiFi — Improv ist nicht erforderlich.

## Nächste Schritte

- [02-claim.md](02-claim.md) — Binden Sie das Gerät an Ihr idryer.org-Konto.
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — wenn sich WiFi nicht verbindet.
