# Datenfluss

Beschreibung, wie Daten in einem laufenden Gerät fließen. Das Ziel ist zu zeigen, dass `idryer-core` weder einen Event Bus noch einen Service Locator verwendet: Teilnehmer sind durch explizite Zeiger in der Composition Root verbunden, und jede Datenrichtung ist ein separater, lesbarer Pfad.

Detaillierte Muster für "wie man Daten zwischen meinen Komponenten transportiert" befinden sich in [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md).

## Hauptrichtungen

```
                Backend / app
                     │
                     │ MQTT commands/*
                     ▼
        ┌──────────────────────────────┐
        │  MqttClient                  │
        │  parses topic + payload      │
        └──────────────┬───────────────┘
                       │
                       │ CommandCallback
                       ▼
        ┌──────────────────────────────┐
        │  IdryerRuntime               │
        │  ping → settimeofday + info  │
        │  others → CommandHandler     │
        └──────────────┬───────────────┘
                       │
                       │ commandHandler_(cmd, data)
                       ▼
        ┌──────────────────────────────┐
        │  Product handleCommand()     │
        │  invoke / set / get_config / │
        │  product-specific commands   │
        └──────┬───────────────┬───────┘
               │               │
               ▼               ▼
   ActionDispatcher        IProfile             Sensor / Peripheral TODO:
   handleInvoke / Set      getConfig            (product code)
                           applyConfig
                           buildInfoJson
```

```
       Sensor (product)            Profile / executor
            │                           │
            │ tick() / read             │ updates state
            ▼                           ▼
       ┌───────────────────────────────────────┐
       │  Product Publisher                    │
       │  (StorageTelemetryPublisher, …)       │
       │  builds JsonDocument                  │
       └────────────────┬──────────────────────┘
                        │
                        │ pub.publishX(doc)
                        ▼
       ┌───────────────────────────────────────┐
       │  DevicePublisher (optional)           │
       │  dual-publish helper: MQTT + Local WS │
       └─────────┬─────────────────────┬───────┘
                 │                     │
                 ▼                     ▼
            MqttClient            LocalAccess (WS)
            broker                LAN client
```

## Eingehende Befehle

1. **MQTT** liefert eine Nachricht im Thema `idryer/{serial}/commands/{cmd}`.
2. `MqttClient::handleMessage` analysiert die Payload als JSON und ruft `CommandCallback` auf.
3. `CommandCallback` wird von `IdryerRuntime` in `begin()` registriert — es akzeptiert `(command, data)`, wobei `command` das Suffix nach `commands/` ist.
4. `IdryerRuntime::onMqttCommand`:
   - Wenn `command == "ping"` — synchronisiert die Zeit und veröffentlicht Info. Wird nicht weitergeleitet.
   - Wenn ein `commandHandler_` registriert ist — leitet alles andere an das Produkt weiter.
   - Ansonsten — eingebauter Fallback-Pfad: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `device.getConfig` → `IProfile::getConfig`.

5. **Local WS** (falls verwendet) akzeptiert `{"type":"command","command":"...","data":{...}}`, packt den Umschlag aus und ruft die gleiche `CommandSink` auf, die für den MQTT-Pfad registriert ist. Ein Handler — zwei Transporte.

## Ausgehende Daten

Die Bibliothek veröffentlicht nichts, wenn nicht danach gefragt. Alle ausgehenden Nachrichten werden vom Produkt initiiert:

| Was | Initiiert von | Über welche API |
|------|-------------|--------------|
| `info` | `IdryerRuntime` (einmal wenn Online und auf `ping`) | `MqttClient::publishInfoJson` |
| `telemetry` | Product Publisher | `MqttClient::publishTelemetry` oder `DevicePublisher::publishTelemetry` |
| `status` | Produktcode bei Zustandsänderung | `MqttClient::publishStatus` oder `DevicePublisher::publishStatus` |
| `config` | `handleCommand` auf `device.getConfig` oder `get_config` | `MqttClient::publishConfig` |
| `events` | Produktcode bei einem Ereignis | `MqttClient::publishEvent` |
| `integrations/status` | `LinkIntegrationsManager` | `MqttClient::publishIntegrationsStatus` |
| `offline` | Broker automatisch (LWT) | Gerät veröffentlicht dies niemals |

## Objektverbindungen in der Composition Root

Verweise zwischen Teilnehmern werden explizit durch Konstruktoren und Setter weitergeleitet. Keine globalen Registries.

```
ArduinoWifiManager     ─┐
ArduinoCredentialStore ─┤
HttpApi (← Http)       ─┼──→ CloudStateMachine ──→ IdryerRuntime ──→ MqttClient
MqttClient             ─┘                              ▲
                                                       │
                                ActionDispatcher ──────┤
                                IProfile         ──────┘

                LocalAccess  ──── (setCommandSink) ────→ same handleCommand
                DevicePublisher (&MqttClient, &LocalAccess)

                Sensor  ──→ Publisher  ──→ DevicePublisher  ──→ MqttClient + LocalAccess
                Executor ←── ActionDispatcher (invoke)  ←── handleCommand
```

Jede Verbindung ist eine Zeile in `main.cpp`. Dies ist die "explizite Composition Root".

## Warum dieses Design

- **Keine Magie**: Um zu verstehen, wie Daten von einem Sensor in die Cloud gelangen, sieht der Leser die Zeigerkette in `main.cpp`. Kein Datenfluss ist hinter einer Fassade verborgen.
- **Flexibilität**: Das Produkt entscheidet, ob es `DevicePublisher` (MQTT + WS), nur MQTT oder seinen eigenen Publisher mit zusätzlicher Logik verwendet.
- **Testbarkeit**: Jeder Knoten ist eine separate Klasse mit expliziten Abhängigkeiten. Knoten können ohne Änderung des restlichen Stacks durch Mocks ersetzt werden.

## Was absichtlich fehlt

- Kein globaler Event Bus oder Message Broker im Gerät.
- Keine automatische Erkennung von "Ich habe einen Sensor, ich veröffentliche seine Daten von selbst".
- Keine Typ-Registry von "Gerät kennt alle seine Telemetrie-Anbieter".

Wenn solche Verbindungen vom Produkt benötigt werden — fügt das Produkt sie in seinem eigenen Produktcode hinzu. Die Bibliothek erzwingt sie nicht.
