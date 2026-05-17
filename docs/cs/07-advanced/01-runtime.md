# IdryerRuntime

`IdryerRuntime` je top-level koordinátor zařízení. Propojuje `CloudStateMachine`, `ActionDispatcher`, `IProfile` a `MqttClient` do jednoho vstupního bodu: `begin()` / `loop()`.

## Konstruktor

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

Všechny čtyři parametry jsou povinné. `profile` může být `nullptr` (runtime kontroluje před voláním jeho metod).

## Spuštění

```cpp
void begin();
```

Provádí:

1. Registruje interní `CommandCallback` v `MqttClient`.
2. Volá `cloud->begin()`.

Volejte jednou v `setup()`, po `setCommandHandler()`.

## Hlavní smyčka

```cpp
void loop();
```

Při každém volání:

1. Volá `cloud->loop()` — posunuje stavový stroj.
2. Volá `profile->loop()` — logika produktu.
3. Při prvním přechodu do Online:
   - Volá `profile->onOnline()`.
   - Volá `profile->buildInfoJson()` a publikuje výsledek na `idryer/{serial}/info` (retained).
4. Při ztrátě Online: resetuje příznak, aby se další přechod Online znovu aktivoval.

## Vestavěné zpracování

### ping

```
commands/ping
```

Vždy zpracováno runtimem — nepředáno `CommandHandler`.

Extrahuje `data["timestamp"]` (formát `"YYYY-MM-DDTHH:MM:SSZ"`), synchronizuje systémový čas prostřednictvím `settimeofday()` a znovu publikuje info payload.

## CommandHandler — jediný bod rozšíření

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

Všechny příchozí příkazy kromě `ping` jsou směrovány do registrovaného `CommandHandler`.

Toto je **jediný oficiální způsob** rozšíření zpracování příkazů. Používá se tak, aby MQTT a lokální WS transport convergovaly do jednoho bodu:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(data["action"] | "", "device.getConfig") == 0))
    {
        // Odpovídej oběma transportům:
        s_pub.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // produktově specifické příkazy...
}

// v setup():
runtime.setCommandHandler(handleCommand);   // MQTT
local.setCommandSink(handleCommand);        // lokální WS
```

!!! note "Pokud není CommandHandler registrován"
    Runtime používá vestavěné směrování: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `invoke device.getConfig` → publikuje config. Toto je výchozí chování — zachováno pro kompatibilitu.

## Online status

```cpp
bool isOnline() const;
```

Vrací `true`, pokud je `CloudStateMachine` ve stavu `Online`.

## Co runtime nedělá

- Nepublikuje telemetrii — to je odpovědnost produktu.
- Nespravuje reconnect MQTT přímo — `CloudStateMachine` to řeší.
- Nezná parametry specifické pro zařízení.
