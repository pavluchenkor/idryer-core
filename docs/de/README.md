---
title: "idryer-core für iDryer-Geräte und DIY-Module"
description: "idryer-core-Dokumentation: ESP32-Gerät, MQTT, WLAN, Cloud-Zustandsmaschine und Befehle für einen Filamenttrockner oder ein 3D-Drucker-Modul verbinden."
---

# idryer-core für iDryer-Geräte und DIY-Module

`idryer-core` ist nützlich, wenn ein DIY-Filamenttrockner, eine beheizte Kammer, ein Beleuchtungssystem oder ein anderes 3D-Drucker-Modul zu einem verwalteten iDryer-Gerät werden soll. Die Bibliothek übernimmt WLAN, MQTT, Befehle, Telemetrie und Portal-Kommunikation; der Produktcode beschreibt nur das Verhalten des konkreten Geräts.

`idryer-core` ist eine C++-Bibliothek (Arduino/PlatformIO) für ESP32-basierte iDryer-Geräte. Sie verwaltet WLAN, MQTT, die Cloud-Zustandsmaschine und das Routing von Befehlen. Das Produkt implementiert nur gerätespezifisches Verhalten.

Dies ist die Dokumentation der **Bibliothek**, nicht eines bestimmten Produkts.
Produktdokumentation liegt in [`docs/ru/`](../../docs/ru/).

---

## Schnellstart

**Drei Dinge, die du implementierst:**

1. `IProfile` implementieren — fünf Methoden (Konfiguration, Info, Loop).
2. `main.cpp` zusammensetzen — statische Objekte, Abhängigkeiten über Konstruktoren übergeben.
3. `handleCommand` registrieren — ein gemeinsamer Handler für MQTT und optional lokalen WS.

**Drei Dinge, die die Bibliothek erledigt:**

1. WLAN → Provisioning → MQTT-Sitzung verwalten.
2. Eingehende Befehle an dein `handleCommand` weiterleiten (`ping` wird intern verarbeitet).
3. Deine `IProfile`-Methoden zum richtigen Zeitpunkt aufrufen.

**Was unverändert bleiben kann:**

- `ArduinoWifiManager`, `ArduinoCredentialStore` und andere `Arduino*`-Klassen — unverändert verwenden, keine Unterklassen nötig.
- `CloudStateMachine` — erstellen und an `IdryerRuntime` übergeben; danach verwaltet sie sich selbst.
- `ActionDispatcher` — Kompatibilitäts-Fallback für invoke/set; bei neuen Produkten läuft die Befehlsverarbeitung über `setCommandHandler()`, nicht über `ActionDispatcher`.

Praktischer Leitfaden: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Arbeitsbeispiele: [`examples/`](../../examples/)

---

## Abschnitte

| Abschnitt | Beschreibung |
|---------|-------------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Zweck der Bibliothek, was sie nicht tut, wer sie verwendet |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Tabelle aller Module: Zweck und Optionalität |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Kurzer Einstieg für neue Entwickler: was verbunden, geflasht und erwartet wird |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | Wie das Produkt den Stack zusammensetzt: Reihenfolge der Objekte, Muster für `main.cpp` |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | Was in die Bibliothek gehört und was ins Produkt |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Datenfluss im laufenden Gerät: eingehende Befehle, ausgehende Nachrichten, Verbindungen |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | Klasse `MqttClient`: Konstruktor, Verbindung, Veröffentlichung |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | Alle MQTT-Topics: Strings, Payloads, retained, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`: was sie koordiniert und welche Befehle sie verarbeitet |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | UART-Brücke für Dual-MCU-Geräte |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker: Einrichtung und Einschränkungen |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Arduino-Implementierungen der Geräteschnittstellen |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | `IProfile`-Schnittstelle, Callbacks, Beispiel `LedStripProfile` |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`: Zweck und Änderungsregeln |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Checkliste zum Aufbau eines neuen Produkts auf `idryer-core` |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | Häufige Probleme: WLAN, Provisioning, MQTT, Befehle, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | Sensor hinzufügen und Messwerte veröffentlichen |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | Peripherie hinzufügen und Befehle empfangen |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | Parallelen Transport hinzufügen (BLE, HTTP, custom) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Praktische Rezepte für Datenübergabe zwischen Sensoren, Peripherie, Profil und Publishern |
