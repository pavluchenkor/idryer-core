# IdryerRuntime

`IdryerRuntime` ist der Top-Level-Geräte-Koordinator. Es verbindet `CloudStateMachine`, `ActionDispatcher`, `IProfile` und `MqttClient` in einen einzelnen Einstiegspunkt: `begin()` / `loop()`.

## Konstruktor

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

Alle vier Parameter sind erforderlich. `profile` kann `nullptr` sein (die Runtime überprüft vor dem Aufruf ihre Methoden).

## Startup

```cpp
void begin();
```

Führt durch:

1. Registriert einen internen `CommandCallback` in `MqttClient`.
2. Ruft `cloud->begin()` auf.

Einmal in `setup()` aufrufen, nach `setCommandHandler()`.

## Hauptschleife

```cpp
void loop();
```

Jeder Aufruf:

1. Ruft `cloud->loop()` auf — rückt die State Machine vor.
2. Ruft `profile->loop()` auf — Produktlogik.
3. Bei dem ersten Übergang zu Online:
   - Ruft `profile->onOnline()` auf.
   - Ruft `profile->buildInfoJson()` auf und veröffentlicht das Ergebnis auf `idryer/{serial}/info` (beibehalten).
4. Bei Verlust von Online: Setzt das Flag zurück, damit der nächste Online-Übergang erneut ausgelöst wird.

## Eingebaute Behandlung

### ping

```
commands/ping
```

Immer vom Runtime behandelt — nicht an `CommandHandler` weitergeleitet.

Extrahiert `data["timestamp"]` (Format `"YYYY-MM-DDTHH:MM:SSZ"`), synchronisiert die Systemzeit über `settimeofday()`, dann veröffentlicht die Info-Payload neu.

## CommandHandler — der einzelne Erweiterungspunkt

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

Alle eingehenden Befehle außer `ping` werden an den registrierten `CommandHandler` weitergeleitet.

Dies ist der **einzige offizielle Weg**, um die Command-Verarbeitung zu erweitern. Wird so verwendet, dass MQTT und lokaler WS Transport zu einem einzelnen Punkt konvergieren:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(data["action"] | "", "device.getConfig") == 0))
    {
        // Respond to both transports:
        s_pub.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // product-specific commands...
}

// in setup():
runtime.setCommandHandler(handleCommand);   // MQTT
local.setCommandSink(handleCommand);        // local WS
```

!!! note "Wenn kein CommandHandler registriert ist"
    Die Runtime verwendet eingebautes Routing: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `invoke device.getConfig` → Konfiguration veröffentlichen. Dies ist das Standardverhalten — behalten für Kompatibilität.

## Online-Status

```cpp
bool isOnline() const;
```

Gibt `true` zurück, wenn `CloudStateMachine` im `Online` Zustand ist.

## Was die Runtime nicht tut

- Veröffentlicht keine Telemetrie — das ist Aufgabe des Produkts.
- Verwaltet MQTT-Neuverbindung nicht direkt — `CloudStateMachine` handhabt das.
- Kennt keine gerätespezifischen Konfigurationsparameter.
