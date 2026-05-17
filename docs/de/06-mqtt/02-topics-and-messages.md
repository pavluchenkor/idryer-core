# MQTT-Themen und Nachrichten

Alle Themen haben die Form `idryer/{serial}/{suffix}`, wobei `{serial}` die Geräteseriennummer ist.

Dieses Dokument beschreibt die Themen und Befehle, die von `MqttClient` aus `idryer-core` implementiert werden. Die vollständige Plattform-Schnittstelle (alle Backend-Befehle für alle Gerätetypen) ist in `contracts/portal_backend_status.md` — dies ist die [Platform Reference].

## Gerät → Backend

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

Veröffentlicht einmal, wenn das Gerät zum ersten Mal Online geht, und erneut beim Empfang eines `ping` Befehls.

Die Payload wird vom Produkt über `IProfile::buildInfoJson()` definiert. Felder, die das Backend mindestens erwartet: `hardwareVersion`, `firmwareVersion`, `timestamp`.

Beispiel für Storage Link:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

### telemetry

```
idryer/{serial}/telemetry    retained=false    interval ~10 s
```

Veröffentlicht vom Produkt über `pub.publishTelemetry()`. Die Bibliothek veröffentlicht nicht automatisch.

Beispiel für Storage Link (Klimasensor):

```json
{
  "units": [
    {"unitId": "U1", "temperature": 23.5, "humidity": 47.2}
  ]
}
```

### status

```
idryer/{serial}/status    retained=true    published on change
```

Veröffentlicht vom Produkt bei Zustandsänderung über `pub.publishStatus()`. Payload wird vom Produkt definiert.

### config

```
idryer/{serial}/config    retained=false    on request
```

Veröffentlicht beim Empfang von `device.getConfig` (invoke) oder als Antwort auf `get_config`. Wird über `pub.publishConfig()` oder `pub.publishConfigRaw()` aufgerufen.

Für große Payloads (> 16000 Bytes) wird in Chunks veröffentlicht: Jeder Chunk enthält `tid`, `idx`, `total`, `last`, `d`.

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

Teilweise Konfigurationsupdate über `pub.publishConfigDelta()`. Das Backend erwartet ein `d` Feld (ein Objekt mit den Änderungen).

### events

```
idryer/{serial}/events    retained=false    on event
```

Veröffentlicht vom Produkt über `pub.publishEvent()`. Die Bibliothek generiert nicht automatisch Ereignisse.

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

Veröffentlicht von `LinkIntegrationsManager`. Enthält den Zustand der aktiven Integrations-Verbindung.

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

Wird vom Broker automatisch gesetzt, wenn die TCP-Verbindung abbricht. Das Gerät veröffentlicht dieses Thema niemals manuell.

## Backend → Gerät

Das Gerät abonniert `idryer/{serial}/commands/#`.

### commands/ping

```
idryer/{serial}/commands/ping
```

Wird direkt von `IdryerRuntime` behandelt — synchronisiert die Systemzeit über `settimeofday()` und veröffentlicht Info neu.

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

Bevorzugter Pfad für Produktaktionen. Die Bibliothek leitet den Befehl an den Produkt-`CommandHandler` weiter (empfohlener Pfad). Falls kein `CommandHandler` registriert ist, fällt der Befehl an das eingebaute `ActionDispatcher` (Fallback).

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

Die eingebaute Aktion `device.getConfig` wird vom Runtime oder Produkthandler behandelt — ruft `IProfile::getConfig()` auf und veröffentlicht das Ergebnis.

### commands/set

```
idryer/{serial}/commands/set
```

Setzt einen einzelnen Konfigurationsparameter. Wird an den Produkt-`CommandHandler` weitergeleitet (empfohlener Pfad). Fallback — eingebauter `ActionDispatcher::handleSet()`, falls kein `CommandHandler` registriert ist.

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

Integrations-Verwaltung. Wird von `LinkIntegrationsManager` über den Produkt-`CommandHandler` behandelt.

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```
