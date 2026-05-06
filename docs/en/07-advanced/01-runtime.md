# IdryerRuntime

`IdryerRuntime` is the top-level device coordinator. It connects `CloudStateMachine`, `ActionDispatcher`, `IProfile`, and `MqttClient` into a single entry point: `begin()` / `loop()`.

## Constructor

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

All four parameters are required. `profile` may be `nullptr` (the runtime checks before calling its methods).

## Startup

```cpp
void begin();
```

Performs:

1. Registers an internal `CommandCallback` in `MqttClient`.
2. Calls `cloud->begin()`.

Call once in `setup()`, after `setCommandHandler()`.

## Main loop

```cpp
void loop();
```

Each call:

1. Calls `cloud->loop()` — advances the state machine.
2. Calls `profile->loop()` — product logic.
3. On the first transition to Online:
   - Calls `profile->onOnline()`.
   - Calls `profile->buildInfoJson()` and publishes the result to `idryer/{serial}/info` (retained).
4. On loss of Online: resets the flag so the next Online transition fires again.

## Built-in handling

### ping

```
commands/ping
```

Always handled by the runtime — not passed to `CommandHandler`.

Extracts `data["timestamp"]` (format `"YYYY-MM-DDTHH:MM:SSZ"`), syncs system time via `settimeofday()`, then re-publishes the info payload.

## CommandHandler — the single extension point

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

All incoming commands except `ping` are directed to the registered `CommandHandler`.

This is the **only official way** to extend command handling. Used so that MQTT and local WS transport converge to a single point:

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

!!! note "If no CommandHandler is registered"
    The runtime uses built-in routing: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `invoke device.getConfig` → publishes config. This is the default behaviour — kept for compatibility.

## Online status

```cpp
bool isOnline() const;
```

Returns `true` if `CloudStateMachine` is in the `Online` state.

## What the runtime does not do

- Does not publish telemetry — that is the product's responsibility.
- Does not manage MQTT reconnection directly — `CloudStateMachine` handles that.
- Does not know about device-specific configuration parameters.
