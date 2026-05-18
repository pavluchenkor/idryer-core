# 數據流

Description of how data moves inside a running device. The goal is to show that `idryer-core` uses neither an event bus nor a service locator: participants are connected by explicit pointers in the composition root, and each data direction is a separate, readable path.

Detailed patterns for "how to route data between my parts" are in [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md).

## 主要方向

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

## 傳入命令

1. **MQTT** delivers a message in topic `idryer/{serial}/commands/{cmd}`.
2. `MqttClient::handleMessage` parses the payload as JSON and calls `CommandCallback`.
3. `CommandCallback` is registered by `IdryerRuntime` in `begin()` — it accepts `(command, data)`, where `command` is the suffix after `commands/`.
4. `IdryerRuntime::onMqttCommand`:
   - If `command == "ping"` — syncs time and publishes info. Not passed further.
   - If a `commandHandler_` is registered — passes everything else to the product.
   - Otherwise — fallback built-in path: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `device.getConfig` → `IProfile::getConfig`.

5. **Local WS** (if used) accepts `{"type":"command","command":"...","data":{...}}`, unwraps the envelope, and calls the same `CommandSink` registered for the MQTT path. One handler — two transports.

## 傳出數據

The library publishes nothing unless asked. All outgoing messages are initiated by the product:

| What | Initiated by | Via which API |
|------|-------------|--------------|
| `info` | `IdryerRuntime` (once when Online and on `ping`) | `MqttClient::publishInfoJson` |
| `telemetry` | product publisher | `MqttClient::publishTelemetry` or `DevicePublisher::publishTelemetry` |
| `status` | product code on state change | `MqttClient::publishStatus` or `DevicePublisher::publishStatus` |
| `config` | `handleCommand` on `device.getConfig` or `get_config` | `MqttClient::publishConfig` |
| `events` | product code on an event | `MqttClient::publishEvent` |
| `integrations/status` | `LinkIntegrationsManager` | `MqttClient::publishIntegrationsStatus` |
| `offline` | broker automatically (LWT) | device never publishes this |

## 組合根中的對象連接

References between participants are passed explicitly through constructors and setters. No global registries.

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

Each connection is one line in `main.cpp`. This is the "explicit composition root".

## 為什麼選擇這種設計

- **No magic**: to understand how data travels from a sensor to the cloud, the reader sees the pointer chain in `main.cpp`. No data flow is hidden behind a facade.
- **Flexibility**: the product chooses whether to use `DevicePublisher` (MQTT + WS), publish only to MQTT, or use its own publisher with additional logic.
- **Testability**: each node is a separate class with explicit dependencies. Nodes can be replaced with mocks without changing the rest of the stack.

## 故意缺失的內容

- No global event bus or message broker inside the device.
- No automatic detection of "I have a sensor, I will publish its data on my own".
- No type registry of "device knows all its telemetry providers".

If such connections are needed by the product — the product adds them in its own product code. The library does not impose them.
