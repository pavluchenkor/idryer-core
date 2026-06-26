# IdryerRuntime

`IdryerRuntime` 是设备的顶层协调器。它把 `CloudStateMachine`、`ActionDispatcher`、`IProfile` 和 `MqttClient` 连接成一个入口点：`begin()` / `loop()`。

## 构造函数

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

四个参数都是必需的。`profile` 可以是 `nullptr`（runtime 会在调用前检查）。

## 启动

```cpp
void begin();
```

执行：

1. 在 `MqttClient` 中注册内部 `CommandCallback`。
2. 调用 `cloud->begin()`。

在 `setup()` 中调用一次，位于 `setCommandHandler()` 之后。

## 主循环

```cpp
void loop();
```

每次调用：

1. 调用 `cloud->loop()` — 推进状态机。
2. 调用 `profile->loop()` — 产品逻辑。
3. 第一次进入 Online 时：
   - 调用 `profile->onOnline()`。
   - 调用 `profile->buildInfoJson()` 并将结果发布到 `idryer/{serial}/info`（retained）。
4. 丢失 Online 状态时：重置标志，以便下一次进入 Online 时再次触发。

## 内置处理

### ping

```
commands/ping
```

始终由 runtime 处理，不会传给 `CommandHandler`。

提取 `data["timestamp"]`（格式 `"YYYY-MM-DDTHH:MM:SSZ"`），通过 `settimeofday()` 同步系统时间，然后重新发布 info payload。

## CommandHandler — 唯一扩展点

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

除 `ping` 以外的所有传入命令都会送到已注册的 `CommandHandler`。

这是扩展命令处理的**唯一官方方式**。这样 MQTT 和本地 WS 传输会汇聚到同一个点：

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

!!! note "如果没有注册 CommandHandler"
    runtime 使用内置路由：`invoke` → `ActionDispatcher`，`set` → `ActionDispatcher`，`invoke device.getConfig` → 发布 config。这是默认行为，为兼容性保留。

## 在线状态

```cpp
bool isOnline() const;
```

如果 `CloudStateMachine` 处于 `Online` 状态，则返回 `true`。

## runtime 不做的事

- 不发布遥测 — 这是产品的职责。
- 不直接管理 MQTT 重连 — 由 `CloudStateMachine` 处理。
- 不知道设备特定的配置参数。
