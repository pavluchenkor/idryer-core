# 数据流

对运行中的设备内部数据移动方式的描述。目标是显示 `idryer-core` 既不使用事件总线也不使用服务定位器：参与者通过组合根中的显式指针连接，每个数据方向是一个单独的、可读的路径。

关于"如何在我的部分之间路由数据"的详细模式在 [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md) 中。

## 主要方向

```
                后端 / 应用
                     │
                     │ MQTT commands/*
                     ▼
        ┌──────────────────────────────┐
        │  MqttClient                  │
        │  解析主题 + 有效负载          │
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
        │  产品 handleCommand()        │
        │  invoke / set / get_config / │
        │  product-specific commands   │
        └──────┬───────────────┬───────┘
               │               │
               ▼               ▼
   ActionDispatcher        IProfile
   handleInvoke / Set      getConfig
                           applyConfig
                           buildInfoJson
```

```
       传感器（产品）          配置文件 / 执行器
            │                           │
            │ tick() / read             │ 更新状态
            ▼                           ▼
       ┌───────────────────────────────────────┐
       │  产品发布者                           │
       │  (StorageTelemetryPublisher, …)       │
       │  构建 JsonDocument                    │
       └────────────────┬──────────────────────┘
                        │
                        │ pub.publishX(doc)
                        ▼
       ┌───────────────────────────────────────┐
       │  DevicePublisher（可选）             │
       │  双发布助手：MQTT + 本地 WS          │
       └─────────┬─────────────────────┬───────┘
                 │                     │
                 ▼                     ▼
            MqttClient            LocalAccess (WS)
            broker                LAN 客户端
```

## 传入命令

1. **MQTT** 在主题 `idryer/{serial}/commands/{cmd}` 中传递消息。
2. `MqttClient::handleMessage` 解析有效负载为 JSON 并调用 `CommandCallback`。
3. `CommandCallback` 由 `IdryerRuntime` 在 `begin()` 中注册——它接受 `(command, data)`，其中 `command` 是 `commands/` 后的后缀。
4. `IdryerRuntime::onMqttCommand`：
   - 如果 `command == "ping"` — 同步时间并发布信息。不进一步传递。
   - 如果注册了 `commandHandler_` — 将其他一切传递给产品。
   - 否则 — 回退内置路径：`invoke` → `ActionDispatcher`、`set` → `ActionDispatcher`、`device.getConfig` → `IProfile::getConfig`。

5. **本地 WS**（如果使用）接受 `{"type":"command","command":"...","data":{...}}`、展开信封、并调用为 MQTT 路径注册的相同 `CommandSink`。一个处理程序——两个传输。

## 传出数据

库不发布任何内容除非被要求。所有传出消息都由产品启动：

| 什么 | 由谁启动 | 通过哪个 API |
|------|---------|-----------|
| `info` | `IdryerRuntime`（在线时一次，在 `ping` 上） | `MqttClient::publishInfoJson` |
| `telemetry` | 产品发布者 | `MqttClient::publishTelemetry` 或 `DevicePublisher::publishTelemetry` |
| `status` | 产品代码在状态更改时 | `MqttClient::publishStatus` 或 `DevicePublisher::publishStatus` |
| `config` | `handleCommand` 在 `device.getConfig` 或 `get_config` 时 | `MqttClient::publishConfig` |
| `events` | 产品代码在事件时 | `MqttClient::publishEvent` |
| `integrations/status` | `LinkIntegrationsManager` | `MqttClient::publishIntegrationsStatus` |
| `offline` | 代理自动（LWT） | 设备从不发布此 |

## 组合根中的对象连接

参与者之间的引用通过构造函数和设置器明确传递。没有全局注册表。

```
ArduinoWifiManager     ─┐
ArduinoCredentialStore ─┤
HttpApi (← Http)       ─┼──→ CloudStateMachine ──→ IdryerRuntime ──→ MqttClient
MqttClient             ─┘                              ▲
                                                       │
                                ActionDispatcher ──────┤
                                IProfile         ──────┘

                LocalAccess  ──── (setCommandSink) ────→ 相同 handleCommand
                DevicePublisher (&MqttClient, &LocalAccess)

                传感器  ──→ 发布者  ──→ DevicePublisher  ──→ MqttClient + LocalAccess
                执行器 ←── ActionDispatcher (invoke)  ←── handleCommand
```

每个连接都是 `main.cpp` 中的一行。这是"显式组合根"。

## 为什么选择这个设计

- **没有魔法**：要理解数据如何从传感器传到云，读者看到 `main.cpp` 中的指针链。没有数据流隐藏在外观后面。
- **灵活性**：产品选择是否使用 `DevicePublisher`（MQTT + WS）、仅发布到 MQTT 或使用具有额外逻辑的自己的发布者。
- **可测试性**：每个节点都是一个具有显式依赖的单独类。节点可以被替换为模拟而不改变堆栈的其余部分。

## 故意缺少什么

- 没有设备内的全局事件总线或消息代理。
- 没有自动检测"我有传感器，我会自己发布其数据"。
- 没有"设备知道所有其遥测提供者"的类型注册表。

如果产品需要这样的连接——产品在其自己的产品代码中添加它们。库不施加它们。
