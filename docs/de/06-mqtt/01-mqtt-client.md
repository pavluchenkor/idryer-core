# MqttClient

`MqttClient` ist der MQTT-Client des Geräts. Es umwickelt `PubSubClient`, verwaltet die Verbindung und leitet eingehende Nachrichten weiter. Alle Themen werden automatisch aus der Seriennummer des Geräts gebildet.

## Initialisierung

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

Wird von `CloudStateMachine` nach erfolgreichem Provisioning aufgerufen. Verbindet sich nicht sofort — setzt Parameter und konfiguriert TLS.

Parameter:

- `serialNumber` — Geräteseriennummer. Wird als MQTT-Client-ID und Benutzername verwendet.
- `token` — Gerätetoken. Wird als MQTT-Passwort verwendet.

Bei Erstellung mit dem Flag `MQTT_USE_TLS=1` konfiguriert der Client `WiFiClientSecure` mit dem Let's Encrypt Root CA (eingebettet in `root_ca.h`).

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // see "Buffer size" below
mqttClient_.setKeepAlive(60);
```

## Puffergröße {#buffer-size}

`PubSubClient` verwendet standardmäßig einen 256-Byte-Puffer — ausreichend nur für kurze Nachrichten. Für iDryer-Geräte ist das zu klein: Die Haupt-"schwere" Payload ist die Gerätekonfiguration (Menü), die in einem Stück auf dem Thema `idryer/{serial}/config` veröffentlicht wird.

`MqttClient` setzt den Puffer auf `MQTT_BUFFER_SIZE` und begrenzt die Chunkgröße für große Konfigurationen auf `MQTT_CONFIG_CHUNK_SIZE`. Beide Konstanten sind in `lib/idryer-core/src/mqtt/mqtt_client.h` definiert:

```cpp
#define MQTT_BUFFER_SIZE        16384  // PubSubClient buffer
#define MQTT_CONFIG_CHUNK_SIZE  16000  // maximum data in one config chunk
```

Beziehung zwischen ihnen:

- `MQTT_BUFFER_SIZE` (16384 Bytes) — Obergrenze für **eine MQTT-Nachricht**. Jede `publish*()` Aufruf mit einer Payload größer als dies wird von `PubSubClient` ohne Versand gelöscht.
- `MQTT_CONFIG_CHUNK_SIZE` (16000 Bytes) — Maximale Größe von `"d"` (der Datenteil) in einem `publishConfigRaw` Chunk. Die 384-Byte-Marge ist für die Chunk-Umschlag reserviert: `{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}` plus das automatisch hinzugefügte `timestamp` Feld.

### Warum 16384

Die Zahl wurde nicht aus ästhetischen Gründen, sondern aus der **maximalen erwarteten Geräte-Payload** gewählt, nämlich der Einstellungs-/Menü-Übertragung:

- Storage Link und Link/iHeater Konfiguration (Menü) wird als JSON mit Escape-Zeichen serialisiert. Ein vollständiger Menü-Schnappschuss passt in ~10–14 KB.
- Die Marge zu 16384 deckt Menü-Wachstum ohne Aufteilung in Chunks ab.
- Der Wert ist ein Vielfaches von 4 KB — praktisch für die Zuweisung auf ESP32.

Wenn Ihr Produkt eine größere Konfiguration hat (z.B. ein erweitertes Menü mit vielen Elementen oder Binärwerten), sind zwei Pfade verfügbar:

1. **Erhöhen Sie `MQTT_BUFFER_SIZE`** — überschreiben Sie über `build_flags` in `platformio.ini`:
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   Behalten Sie die RAM-Nutzung im Auge: `PubSubClient` hält diesen Puffer kontinuierlich. Auf ESP32-C3 (~400 KB freier Heap) sind 32 KB akzeptabel, aber weiter zu gehen trägt Risiken.

2. **Verwenden Sie `publishConfigRaw(json, length)`** — sie teilt die Payload in Chunks von `MQTT_CONFIG_CHUNK_SIZE` auf; das Backend setzt sie über die Felder `tid` / `idx` / `total` / `last` wieder zusammen. Dieser Pfad ist vorzuziehen für Konfigurationen, die von RP2040 über UART in Stücken beliebiger Länge kommen.

### Gilt für Produktveröffentlichungen

Die gleiche 16384-Byte-Obergrenze gilt für `publishTelemetry`, `publishStatus`, `publishEvent`. In der Praxis sind Telemetrie und Events viel kleiner (hunderte von Bytes); nur Konfigurationsveröffentlichungen nähern sich dieser Grenze. Wenn Ihr Projekt regelmäßig eine große Payload veröffentlicht (z.B. ein Messpuffer-Dump), schätzen Sie seine Größe voraus oder teilen Sie ihn selbst auf.

## Verbindung

```cpp
bool MqttClient::connect();
```

Führt durch:

1. Verbindung zum Broker mit persistenter Sitzung (`clean_session = false`). Persistente Sitzung ist obligatorisch — ohne sie gehen Befehle, die ankommen, während das Gerät offline ist, verloren.
2. Setzt die LWT-Nachricht auf Thema `idryer/{serial}/offline` (QoS 1, nicht beibehalten).
3. Abonniert `idryer/{serial}/commands/#` (QoS 1). Macht bis zu 3 Versuche; bei Fehler wird die Verbindung getrennt.

Gibt `true` zurück, wenn Verbindung und Abonnement erfolgreich waren.

## Loop

```cpp
void MqttClient::loop();
```

Wird bei jeder Iteration aufgerufen. Verbindet sich bei Trennung neu, dann ruft `PubSubClient::loop()` auf, um eingehende Nachrichten zu empfangen.

## Veröffentlichung

Alle Veröffentlichungsmethoden fügen ein `timestamp` Feld (ISO 8601 UTC) hinzu, falls es nicht bereits im Dokument vorhanden ist.

| Methode | Thema | Beibehalten |
|--------|-------|----------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | ja |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | nein |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | ja |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | nein |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | nein |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | ja |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | nein |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | nein |

`publishConfigRaw` teilt die Payload automatisch in Chunks auf, wenn die Größe `MQTT_CONFIG_CHUNK_SIZE` (16000 Bytes) überschreitet. Jeder Chunk enthält die Felder `tid`, `idx`, `total`, `last`, `d`.

!!! note
    `PubSubClient` veröffentlicht immer mit QoS 0, unabhängig von den Thema-Einstellungen. Dies ist eine Bibliotheks-Einschränkung.

## Empfangen von Befehlen

Eingehende Nachrichten in Thema `idryer/{serial}/commands/{cmd}` werden als JSON analysiert und an den registrierten `CommandCallback` weitergeleitet:

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

Der `{cmd}` Teil wird aus dem Thema extrahiert und als erstes Argument weitergeleitet. `IdryerRuntime` registriert diesen Callback in `begin()`.

## Hilfsmethoden

```cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bytes
static char* generateUuid(char* buffer);    // buffer >= 37 bytes
```

`generateUuid` generiert eine UUID v4 basierend auf `esp_random()`.

## Einschränkungen

- Eine `MqttClient` Instanz pro Gerät (Singleton über `instance_`).
- Maximale Größe einer einzelnen JSON-Nachricht — `MQTT_BUFFER_SIZE` (Standard 16384 Bytes). Dimensioniert für die schwerste Geräte-Payload — typischerweise die serialisierte Konfiguration (Menü). Für größere Konfigurationen erhöhen Sie die Konstante über `build_flags` oder verwenden Sie `publishConfigRaw` mit automatischer Chunk-Aufteilung. Siehe [Puffergröße](#buffer-size).
- TLS wird durch das Build-Flag `MQTT_USE_TLS` aktiviert.
