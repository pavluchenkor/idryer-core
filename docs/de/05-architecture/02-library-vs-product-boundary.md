# Grenze: Bibliothek und Produkt

## Was in der Bibliothek lebt

Die Bibliothek (`lib/idryer-core/`) enthält:

- Den gesamten Netzwerk-Stack: WiFi, HTTP, MQTT, TLS.
- Das Provisioning-/Claim-Protokoll.
- Die Cloud State Machine (`CloudStateMachine`).
- UART Bridge und Frame-Protokoll.
- Integrations-Clients (Bambu, HA, Moonraker).
- Geräte-Schnittstellen (`IWifiManager`, `ICredentialStore`, `IHttpClient`, `IProfile`).
- Arduino-Implementierungen dieser Schnittstellen.
- MQTT-Themen und Publish/Subscribe Logik.

Der Test für Code, der in die Bibliothek gehört: **Jedes Produkt mit jeglicher Hardware kann es ohne Änderung verwenden**.

## Was im Produkt lebt

Das Produkt (`src/`) enthält:

- `IProfile` Implementierung — Konfiguration, Info-Payload, `applyConfig`.
- Geschäftslogik spezifisch für das Gerät (LED-Steuerung, Trocknung, Heizung).
- `onInvoke` / `onSetCommand` Handler.
- Produkt-Sensoren und Telemetrie-Veröffentlichung.
- Peripherie-Initialisierung (FastLED, Wire, ImprovWiFi).
- Composition Root in `main.cpp`.

Der Test für Code, der in das Produkt gehört: **Ohne Änderung der Hardware oder Konfiguration ist es bedeutungslos**.

## Konkrete Beispiele

| Code | Wo er lebt | Warum |
|------|---------------|-----|
| `MqttClient` | Bibliothek | Jedes Produkt braucht MQTT |
| `CloudStateMachine` | Bibliothek | Provisioning/Claiming ist gleich für alle |
| `ArduinoWifiManager` | Bibliothek | WiFi-Verbindung hängt nicht vom Produkt ab |
| `LedStripProfile` | Produkt | Spezifisch für Storage Link TODO: use consistent Storage name throughout the doc |
| `LedStripExecutor` | Produkt | Steuert FastLED, nicht benötigt von anderen Geräten |
| `Sht31ClimateSensor` | Produkt | Ein spezifischer Sensor für ein spezifisches Produkt |
| `StorageTelemetryPublisher` | Produkt | Kennt das Storage Link Telemetrie-Format |
| `IProfile` | Bibliothek | Vertrag, den die Bibliothek aufruft |
| `BambuClient` | Bibliothek | Integration wird über iDryer und iHeater wiederverwendet |

## Schnittstellen als Grenze

Die Bibliothek kennt das Produkt nur durch `IProfile`. Die ganze Interaktion geht durch fünf Methoden:

```cpp
profile->onOnline();               // library → product: first time going online
profile->loop();                   // library → product: every cycle
profile->buildInfoJson(buf, len);  // library → product: info payload needed
profile->getConfig(doc);           // library → product: config needed
profile->applyConfig(id, val);     // library → product: set command received
```

Das Produkt kennt die Bibliothek durch `MqttClient` (zum Veröffentlichen von Telemetrie/Events) und durch `ActionDispatcher` Callbacks (für Befehle).

## Was die Grenze nicht überschreiten darf

- Die Bibliothek darf keine Produktheader einbinden.
- Das Produkt darf nicht direkt `CloudStateMachine::handleProvisioning()` oder andere private Stack-Methoden aufrufen — nur über die öffentliche API.
- Produkt-Telemetrie wird direkt über `s_mqtt.publishTelemetry()` veröffentlicht — die Runtime sieht es nicht.
