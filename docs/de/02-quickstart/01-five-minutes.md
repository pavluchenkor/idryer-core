---
title: "Ein Gerät auf idryer-core in 5 Minuten starten"
description: "Das erste Gerät auf idryer-core: PlatformIO-Projekt, minimale Firmware, WLAN und Kopplung mit dem Konto in der iDryer-App."
---

# Ein Gerät auf idryer-core in 5 Minuten starten

Nach dieser Seite ist der ESP32 im Netz, mit Ihrem Konto gekoppelt und im Portal [portal.idryer.org](https://portal.idryer.org/) sowie in der iDryer-App sichtbar. Sie brauchen: ein ESP32-C3-Board (DevKit, Super Mini oder kompatibel), ein USB-Kabel, PlatformIO in VS Code, ein Telefon mit der iDryer-App, ein WLAN mit 2,4 GHz.

## 1. PlatformIO-Projekt

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← Kopie, Git-Submodul oder symbolischer Link
└── src/
    └── main.cpp
```

`platformio.ini`:

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

Die Bibliotheken des Kerns (MQTT, ArduinoJson, WebSockets, Improv) kommen von selbst aus der `library.json` des Kerns. Ein Board mit USB-UART braucht die letzten beiden Flags nicht; `board` passend zu Ihrem Board setzen.

## 2. Code

Kopieren Sie das Beispiel [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) nach `src/main.cpp`:

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // eigenes Gerät
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Blinkperiode nach Zustand; 0 = nicht blinken.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Entkopplung: der Kern löscht das Geheimnis und wartet wieder auf die Kopplung.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // Telemetriefelder veröffentlicht der Kern selbst, alle 30 s (im Leerlauf alle 60 s).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

Die Firmware enthält weder ein WLAN-Passwort noch Kontodaten: Netz und Kopplung bekommt das Gerät aus der App.

## 3. Flashen und Log öffnen

```bash
pio run -t upload
pio device monitor -b 115200
```

Solange das Gerät kein WLAN hat, schweigt das Log: der Kern hält den Port für Improv. Die LED blinkt schnell: das Gerät wartet auf Netz und Kopplung.

## 4. WLAN und Kopplung in der App

1. Verbinden Sie das Telefon mit dem WLAN, in dem das Gerät arbeiten soll, und melden Sie sich in der iDryer-App mit Ihrem Portalkonto an.
2. Tippen Sie auf dem Startbildschirm auf **Neues Gerät verbinden**: der Schritt **WLAN** öffnet sich.
3. Prüfen Sie den Netzwerknamen, geben Sie das Passwort ein und tippen Sie auf **Gerät verbinden**. Die App sendet die Einstellungen bis zu 90 Sekunden lang; sobald das Gerät im Netz ist, erscheint **Gerät verbunden**. Tippen Sie auf **Weiter**.
4. Tippen Sie im Schritt **Kopplung** auf **Koppeln**. Die App findet das Gerät im Netz, holt beim Portal ein einmaliges Kopplungstoken und übergibt es dem Gerät.
5. Nach **Gerät gekoppelt** erscheint das Gerät in der Geräteliste im Portal und in der App.

Das Log nach dem Beitritt zum Netz:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Nach der Kopplung:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Prüfen

- das Gerät ist im Portal und in der App online;
- die Karte zeigt die Chiptemperatur des ESP32;
- die LED blinkt einmal pro Sekunde. Dauerhaft an oder aus bedeutet: keine Verbindung zum Portal.

## Wenn es nicht klappt

- die App hat die Verbindung des Geräts nicht abgewartet: Passwort prüfen und ob das Netz 2,4 GHz hat; bei falschem Passwort wartet das Gerät wieder auf Einstellungen. Andere Wege, das Netz zu übergeben: [WLAN](01-wifi.md);
- im Schritt **Kopplung** wurde das Gerät nicht gefunden: Telefon und Gerät müssen im selben Netz sein; Gastnetze blockieren oft die Gerätesuche. Mehr: [Kopplung mit dem Konto](02-claim.md);
- Build und Flags: [Ausführliche Einrichtung](99-detailed-setup.md).

## Weiter

- [Telemetrie](03-telemetry.md): ein Sensor und ein eigener Wert auf der Karte.
- [Beispiele des Kerns](https://github.com/pavluchenkor/idryer-core/tree/main/examples): Kartenaktionen, eigene Sensoren und Bedienelemente.
