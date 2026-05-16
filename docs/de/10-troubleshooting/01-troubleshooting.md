# Fehlerbehebung

Häufige Symptome bei der Arbeit mit `idryer-core`, ihre Ursachen und Lösungen.

Stellen Sie sicher, dass HAL-Protokolle aktiviert sind (`idryer::hal::initArduinoHal(&Serial)`) und dass `-DCORE_DEBUG_LEVEL=3` oder höher in `platformio.ini` gesetzt ist.

## WiFi

### State Machine steckt in `WifiConnecting`

Symptome: Protokoll wiederholt `state: WifiConnecting`, Übergang zu `Provisioning` geschieht nie.

Mögliche Ursachen:

- Falsche SSID/Passwort. Überprüfen Sie `WIFI_SSID` / `WIFI_PASSWORD` in `secrets.h`. Nach Improv-Provisioning kommen Anmeldedaten aus NVS, nicht von `secrets.h`.
- 5-GHz-Netzwerk. ESP32 unterstützt nur 2,4 GHz.
- Verstecktes Netzwerk oder MAC-Filter auf dem Router.
- `WiFi.begin()` aufgerufen vor `idryer::hal::initArduinoHal(...)` — Keine Protokollausgabe, aber dies ist nicht die Ursache des Hängens, nur Blindheit.

Was zu überprüfen ist:

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFi verbindet sich, bricht aber nach 30–60 Sekunden ab

Typischerweise: schwaches Signal (`RSSI < -80 dBm`), ESP32-C3 wird von einem USB-Hub ohne dediziertes 5V/1A-Netzteil betrieben, Konflikt mit FreeRTOS-Aufgaben.

Protokollieren Sie RSSI in der Produkt-Loop:

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## Provisioning und Claiming

### State Machine steckt in `Provisioning`

Symptome: `state: Provisioning` ohne Übergang zu `Registering` oder `AwaitingClaim`.

Ursachen:

- Falsche `IDRYER_API_BASE` in Build-Flags. Muss `https://portal.idryer.org/api` (Produktion) oder `https://staging.idryer.org/api` (Staging) sein.
- Fehlendes TLS-Zertifikat (Let's Encrypt ISRG Root X1). Eingebettet in `root_ca.h`, aber wenn ohne `MQTT_USE_TLS` erstellt, benötigt der HTTP-Client auch TLS — das Root CA wird benötigt für die HTTP API auch.
- Gerätezeit nicht synchronisiert (TLS-Handshake benötigt ein gültiges Datum). Überprüfen Sie, dass `configTime(...)` in `setStateChangeCallback` nach dem ersten Verlassen von `WifiConnecting` aufgerufen wird (wie in Storage Link).

### State Machine steckt in `AwaitingClaim`

Dies ist der normale Zustand, während der Benutzer die PIN im Portal nicht eingegeben hat. Die PIN wird über `setClaimPinCallback` ins Protokoll ausgegeben.

Für automatisches Claiming (eigenständige Geräte ohne UI):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Nach `requestClaim()` gibt das Backend eine PIN aus, die der Benutzer ins Portal eingeben muss.

### `seedSerialFromMac()` generierte eine Seriennummer, aber eine andere wurde ins Portal eingegeben

Die in NVS gespeicherte Seriennummer hat Vorrang vor MAC-Generierung. `seedSerialFromMac()` schreibt in NVS nur, wenn noch keine Seriennummer vorhanden ist. Um die Seriennummer zu ändern, löschen Sie NVS:

```cpp
s_credentials.clear();
```

## MQTT

### State Machine betrat `MqttConnecting`, aber erreicht `Online` nicht

Ursachen:

- Broker unerreichbar. Produktion: `mqtt.idryer.org:8883`, Staging: `staging.idryer.org:1884`.
- `MQTT_USE_TLS=1` ohne korrektes Root CA — Handshake schlägt stumm fehl.
- `setBufferSize(16384)` nicht angewendet — `PubSubClient` Puffer ist Standard 256 Bytes. `MqttClient` setzt bereits 16384, aber wenn Sie `PubSubClient` direkt verwenden — setzen Sie den Puffer selbst.
- Persistente Sitzung "steckt" auf dem Broker mit einer anderen Client-ID. Löschen Sie NVS und reflashen Sie.

### Befehle vom Backend kommen nicht an

Überprüfen Sie das Abonnement — `MqttClient` abonniert `idryer/{serial}/commands/#` mit QoS 1. Wenn das Abonnement fehlschlug, zeigt das Protokoll:

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

Überprüfen Sie, dass `setCommandHandler()` **vor** `runtime.begin()` aufgerufen wird — sonst wird die erste Batch von Befehlen möglicherweise verpasst.

### `PubSubClient` trennt sich genau alle 60 Sekunden

Dies ist ein Keep-Alive-Timeout. Ihre MQTT-Loop wird möglicherweise nicht häufig genug aufgerufen — `s_runtime.loop()` muss ohne lange Blöcke spinnen. Überprüfen Sie, dass `loop()` kein `delay(>500ms)` und keine blockierenden Netzwerkaufrufe hat.

## Befehle und Handler

### `commands/invoke` kommt an, aber `ActionDispatcher` wird nicht aufgerufen

Wenn Sie `setCommandHandler()` registrierten, **ist der eingebaute Fallback zu `ActionDispatcher` deaktiviert**. `IdryerRuntime` leitet alles (außer `ping`) zu Ihrem `CommandHandler`. Sie müssen dort explizit `s_dispatcher.handleInvoke(data)` für `invoke` Befehle aufrufen.

Vorlage:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... product commands ...
}
```

### `commands/set` empfangen, aber Konfiguration nicht angewendet

`ActionDispatcher::handleSet` extrahiert `id` und `val` und leitet sie an den registrierten `SetCallback` weiter. Überprüfen Sie:

- `dispatcher.setSetCallback(onSetCommand, nullptr)` wird in `setup()` aufgerufen.
- `onSetCommand` ruft tatsächlich `s_profile.applyConfig(id, val)` auf.
- `applyConfig` gibt `true` für bekannte `id` Werte zurück. Für unbekannte gibt es `false` zurück und Änderungen werden ignoriert.

## Telemetrie

### Telemetrie wird nicht veröffentlicht

`idryer-core` veröffentlicht Telemetrie nicht automatisch. Der Produktcode tut dies immer.

Überprüfen Sie:

- `pub.publishTelemetry(doc)` (oder `s_mqtt.publishTelemetry(doc)`, falls LocalAccess nicht verwendet wird) wird tatsächlich in `loop()` aufgerufen.
- Die Ratenbedingung blockiert nicht alle Aufrufe. Ein häufiger Fehler:
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  Beim ersten Durchlauf ist `lastTm == 0` und `millis()` ist noch klein — der Zweig wird nie ausgeführt. Verwenden Sie `>=` und initialisieren Sie `lastTm` beim ersten Durchlauf.
- `s_runtime.isOnline() == true`. MQTT ist getrennt, bevor Online — Veröffentlichung wird nicht durchgehen.
- `JsonDocument` Größe ist ausreichend für die Payload. Überprüfen Sie `doc.overflowed()` nach `serializeJson`.

### `publishTelemetry` gibt `false` zurück

Ursachen:

- Nicht mit Broker verbunden (`MqttClient::isConnected() == false`).
- Puffer überschritten — Payload größer als `MQTT_BUFFER_SIZE` (16384 Bytes). Für große Daten verwenden Sie `publishConfigRaw` (mit Chunks) oder reduzieren Sie die Payload.

### `DevicePublisher::publishTelemetry` erreicht den WS-Client nicht

`DevicePublisher` gibt keinen Fehler zurück, wenn der WS-Client nicht verbunden ist — es überspringt einfach den WS-Teil. Überprüfen Sie `s_local.isClientConnected()`. Falls `false` — der Client ist nicht authentifiziert oder nicht verbunden.

## NTP und Systemzeit

### Gerätezeit ist nicht synchronisiert

NTP-Synchronisierung wird in `setStateChangeCallback` nach dem ersten Verlassen von `WifiConnecting` gestartet:
