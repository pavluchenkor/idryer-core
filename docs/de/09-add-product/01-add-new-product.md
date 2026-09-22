---
title: "Neues Produkt auf Basis von idryer-core hinzufügen"
description: "Checkliste für ein neues iDryer-Gerät auf der Fassade iDryer::Link: Projekt, minimale Firmware, WLAN und Kopplung in der App, Telemetrie, Einstellungsmenü, Gerätekarte, Vertrag."
---

# Neues Produkt auf Basis von idryer-core hinzufügen

Nutzen Sie diese Anleitung, wenn Sie ein neues Produkt auf `idryer-core` bauen: einen Filamenttrockner, einen Heizblock, eine Beleuchtung, einen Sensor oder ein anderes Modul. Sie zeigt, was der Core für Sie erledigt und was der Produktcode ergänzen muss.

Ein vollständiges, baubares Beispiel ist der beheizte Lagerschrank aus der Dokumentation Build-Your-Own-iDryer (`example/09-cabinet`): er geht alles auf dieser Seite durch.

---

## Worauf ein Produkt aufbaut

Ein Produkt spricht mit dem Core über ein einziges Objekt — die Fassade `iDryer::Link` (`<iDryer.h>`). In `s_link.begin()` und `s_link.loop()` erledigt der Core:

- er übernimmt das WLAN aus der iDryer-App per Funk (ESPTouch) oder aus dem Web-Installer per USB (Improv) und hält die Verbindung;
- er koppelt das Gerät an ein Konto: wartet auf ein einmaliges Kopplungstoken — aus der App über das lokale Netz oder über die serielle Schnittstelle (`PAIR_TOKEN:<token>`) — und tauscht es im Portal gegen ein dauerhaftes Geheimnis;
- er verbindet sich mit MQTT und veröffentlicht `telemetry` und `status` in den Perioden aus `Config`;
- er meldet sich im lokalen Netz an (mDNS `_idryer._tcp`) und nimmt Befehle der App über WebSocket an;
- er veröffentlicht das Card-Manifest der Gerätekarte.

Die Klassen darunter (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` und andere) sind Interna des Cores: das Kopplungstoken erreicht den Cloud-Teil nur innerhalb von `iDryer::Link`. Bauen Sie ein Produkt auf der Fassade.

---

## 1. Projekt

`idryer-core` kommt nach `lib/idryer-core/` (Kopie oder symbolischer Link); die Bibliotheken des Cores holt PlatformIO aus dessen `library.json`. Minimale `platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESP8266-Transport aus den Abhängigkeiten von espMqttClient: baut nicht auf ESP32
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Ohne `lib_ignore = ESPAsyncTCP` bricht der Build in `ESPAsyncTCP.cpp` ab; ohne `MQTT_BROKER` und `MQTT_PORT` kompiliert der Core nicht.

---

## 2. Minimale Firmware

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // kein iDryer-Produkt: die Karte kommt aus dem Manifest
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // Gerät im Portal entkoppelt: Geheimnis löschen, auf neue Kopplung warten.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // Ihre Sensorfunktionen
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` startet WLAN, Kopplung, MQTT und lokalen Zugriff; `s_link.loop()` muss ständig laufen, ohne `delay()`. Der Befehl `revoke` kommt vom Portal, wenn das Gerät vom Konto entkoppelt wird: `handleRevoke()` löscht das Geheimnis, und das Gerät wartet auf ein neues Kopplungstoken.

---

## 3. WLAN und Kopplung — nichts im Code

Die Firmware enthält weder das WLAN-Passwort noch Kontodaten. Der Nutzer verbindet das Gerät in der iDryer-App: **Neues Gerät verbinden** → Schritt **WLAN** (die App sendet das Netz per ESPTouch) → Schritt **Kopplung** (die App findet das Gerät per mDNS, holt beim Portal ein einmaliges Token und übergibt es dem Gerät; das Gerät aktiviert das Token im Portal selbst). Bis das WLAN steht, bleibt die serielle Schnittstelle still: der Core hält sie für den Web-Installer (Improv) frei.

Schritt für Schritt und mit dem erwarteten Log — Build-Your-Own-iDryer, Kapitel „Firmware-Start auf dem Core“.

---

## 4. Daten: Telemetrie und Status

- Die Flags `has*` in `Config` legen fest, welche Vokabularfelder in die Telemetrie gehen und welche Zellen auf der Karte erscheinen.
- Schreiben Sie Werte in `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) und `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); der Core veröffentlicht sie in den Perioden aus `Config`, `s_link.publishStatusNow()` sendet den Status sofort.
- Ein eigenes Feld — über `s_link.onTelemetryPublish()`, auf die Karte — über `s_link.card().sensor()`; siehe [Gerätekarte](02-add-widget.md).

---

## 5. Einstellungen: das Menü

Einstellungen werden in `src/menu/menu.yaml` beschrieben; der Generator `menu_gen.py` macht daraus C++-Code, die Speicherung im NVS und das Menü-JSON ([Menü als Protokoll](../08-contracts/02-menu-as-protocol.md)). Der Produktcode:

- lädt das Menü vor `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- veröffentlicht es mit `menu_buildFullJson()` und `s_link.devicePublisher()->publishConfigRaw()` — beim Online-Gehen und auf den Befehl `get_config`;
- übernimmt den Befehl `set` mit `menu_apply_by_bind()` (Wert, NVS und Cache auf einmal) und veröffentlicht das Menü erneut.

Der vollständige Code — Build-Your-Own-iDryer, Kapitel „Menü aus YAML“.

---

## 6. Gerätekarte

Was die Karte zeigt und welche Operationen sie startet, wird mit `s_link.card()` deklariert — [Gerätekarte: das Card-Manifest](02-add-widget.md).

---

## 7. Vertrag

Wenn Sie neue Topics hinzufügen oder Payloads ändern:

1. aktualisieren Sie `contracts/mqtt_contract.yaml`;
2. führen Sie `contracts/regen.sh` aus und committen Sie die erzeugten Dateien.

---

## Zwei-Chip-Geräte

Für einen ESP32, der über UART mit einem eigenen Controller arbeitet (zum Beispiel RP2040), hat der Core die UART-Brücke `idryer_uart.h`; die funktionierende Vorlage ist die Firmware `idryer-link`.
