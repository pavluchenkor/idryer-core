# 数据流

本页描述数据如何在运行中的设备内部移动。目标是说明 `idryer-core` 既不使用事件总线，也不使用 service locator：参与者在组合根中通过显式指针连接，每个数据方向都是独立且可读的路径。

“如何在我的部件之间路由数据”的详细模式见 [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md)。

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

## 传入命令

1. **MQTT** 在 `idryer/{serial}/commands/{cmd}` topic 中送达消息。
2. `MqttClient::handleMessage` 将 payload 解析为 JSON，并调用 `CommandCallback`。
3. `CommandCallback` 由 `IdryerRuntime` 在 `begin()` 中注册；它接收 `(command, data)`，其中 `command` 是 `commands/` 后面的后缀。
4. `IdryerRuntime::onMqttCommand`：
   - 如果 `command == "ping"` — 同步时间并发布 info，不再继续传递。
   - 如果注册了 `commandHandler_` — 把其他所有命令传给产品。
   - 否则走内置 fallback 路径：`invoke` → `ActionDispatcher`，`set` → `ActionDispatcher`，`device.getConfig` → `IProfile::getConfig`。

5. **Local WS**（如果使用）接收 `{"type":"command","command":"...","data":{...}}`，拆开 envelope，并调用与 MQTT 路径相同的 `CommandSink`。一个 handler，两种传输。

## 传出数据

除非被要求，库不会发布任何内容。所有传出消息都由产品发起：

| 内容 | 发起方 | 使用的 API |
|------|-------------|--------------|
| `info` | `IdryerRuntime`（Online 时一次，收到 `ping` 时一次） | `MqttClient::publishInfoJson` |
| `telemetry` | 产品 publisher | `MqttClient::publishTelemetry` 或 `DevicePublisher::publishTelemetry` |
| `status` | 状态变化时的产品代码 | `MqttClient::publishStatus` 或 `DevicePublisher::publishStatus` |
| `config` | `device.getConfig` 或 `get_config` 上的 `handleCommand` | `MqttClient::publishConfig` |
| `events` | 事件发生时的产品代码 | `MqttClient::publishEvent` |
| `integrations/status` | `LinkIntegrationsManager` | `MqttClient::publishIntegrationsStatus` |
| `offline` | broker 自动设置（LWT） | 设备从不发布 |

## 组合根中的对象连接

参与者之间的引用通过构造函数和 setter 显式传递。没有全局 registry。

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

每条连接都是 `main.cpp` 中的一行。这就是“显式组合根”。

## 为什么选择这种设计

- **没有魔法**：要理解数据如何从传感器到云端，读者只需查看 `main.cpp` 中的指针链。没有数据流隐藏在 facade 后面。
- **灵活性**：产品决定是使用 `DevicePublisher`（MQTT + WS）、只发布到 MQTT，还是使用带额外逻辑的自定义 publisher。
- **可测试性**：每个节点都是带显式依赖的独立类。可以用 mock 替换节点，而不改变栈的其他部分。

## 有意不提供的内容

- 设备内部没有全局事件总线或消息 broker。
- 不会自动检测“我有一个传感器，我会自己发布它的数据”。
- 没有“设备知道所有遥测提供者”的类型 registry。

如果产品需要这些连接，产品应在自己的产品代码中添加。库不会强制它们。
