# MQTT-Vertrag

Die Datei `contracts/mqtt_contract.yaml` ist die Quelle der Wahrheit für die `idryer-core` MQTT-Schnittstelle.

## Umfang

Der Vertrag beschreibt **nur, was `MqttClient` von `idryer-core` implementiert**:

- Themen, die die Bibliothek veröffentlichen kann
- Befehle, die die Bibliothek akzeptiert und leitet

Die vollständige Plattform-Schnittstelle (alle Backend-Befehle für alle Gerätetypen, einschließlich `drying`, `storage`, `profile`, `rfid` usw.) ist in `contracts/portal_backend_status.md` — dies ist die [Platform Reference].

## Geräte-Themen (Gerät → Backend)

| Suffix | Beibehalten | Status |
|--------|----------|--------|
| `info` | ja | implementiert |
| `telemetry` | nein | implementiert |
| `status` | ja | implementiert |
| `config` | nein | implementiert |
| `config/delta` | nein | implementiert |
| `events` | nein | implementiert |
| `integrations/status` | ja | implementiert |
| `offline` (LWT) | nein | implementiert |

## Befehle (Backend → Gerät)

| Suffix | Handler | Status |
|--------|---------|--------|
| `commands/ping` | `IdryerRuntime` (eingebaut) | implementiert |
| `commands/invoke` | Produkt `CommandHandler` (emp.); Fallback → `ActionDispatcher` | implementiert |
| `commands/set` | Produkt `CommandHandler` (emp.); Fallback → `ActionDispatcher` | implementiert |
| `commands/link_integration` | `LinkIntegrationsManager` über `CommandHandler` | implementiert |
| `commands/bambu_apply` | `LinkIntegrationsManager` über `CommandHandler` | implementiert |
| Alle anderen | Produkt `CommandHandler` | produktdefiniert |

## Änderungsregel

Jede Änderung am MQTT-Protokoll in `idryer-core` muss gleichzeitig folgende Punkte ändern:

1. `contracts/mqtt_contract.yaml`
2. Bibliothekscode (`mqtt_client.h/.cpp`)
3. Portal / Backend-Code

Aktualisieren Sie zuerst den Vertrag, dann den Code.

## Kompatibilität

- Das Hinzufügen neuer optionaler Felder zu einer Payload ist sicher.
- Das Umbenennen bestehender Felder erfordert gleichzeitige Updates auf Firmware, Portal und Vertrag.
- `info` und `config` Payloads werden vom Produkt definiert — `idryer-core` validiert sie nicht.
