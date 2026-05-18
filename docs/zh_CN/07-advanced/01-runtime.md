# IdryerRuntime

`IdryerRuntime` 是顶级设备协调器。它将 `CloudStateMachine`、`ActionDispatcher`、`IProfile` 和 `MqttClient` 连接到一个单一入口点：`begin()` / `loop()`。

## 构造函数

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

所有四个参数都是必需的。`profile` 可能是 `nullptr`（运行时在调用其方法前检查）。

## 启动

```cpp
void begin();
```

执行：

1. 在 `MqttClient` 中注册内部 `CommandCallback`。
2. 调用 `cloud->begin()`。

在 `setup()` 中调用一次，在 `setCommandHandler()` 之后。

## 主循环

```cpp
void loop();
```

每次调用：

1. 调用 `cloud->loop()` — 推进状态机。
2. 调用 `profile->loop()` — 产品逻辑。
3. 在第一次转换到在线时：
   - 调用 `profile->onOnline()`。
   - 调用 `profile->buildInfoJson()` 并将结果发布到 `idryer/{serial}/info`（保留）。
4. 在失去在线时：重置标志以便下一个在线转换再次触发。

## 内置处理

### ping

```
commands/ping
```

总是由运行时处理——不传递给 `CommandHandler`。

提取 `data["timestamp"]`（格式 `"YYYY-MM-DDTHH:MM:SSZ"`），通过 `settimeofday()` 同步系统时间，然后重新发布信息有效负载。

## CommandHandler — 唯一的扩展点

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

除 `ping` 外的所有传入命令都被定向到注册的 `CommandHandler`。

这是**唯一官方方式**来扩展命令处理。使用以便 MQTT 和本地 WS 传输汇聚到一个点：

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(data["action"] | "", "device.getConfig") == 0))
    {
        // 响应两个传输：
        s_pub.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // 产品特定的命令...
}

// 在 setup() 中：
runtime.setCommandHandler(handleCommand);   // MQTT
local.setCommandSink(handleCommand);        // 本地 WS
```

!!! note "如果没有注册 CommandHandler"
    运行时使用内置路由：`invoke` → `ActionDispatcher`、`set` → `ActionDispatcher`、`invoke device.getConfig` → 发布配置。这是默认行为——保留以保持兼容性。

## 在线状态

```cpp
bool isOnline() const;
```

如果 `CloudStateMachine` 处于 `Online` 状态，返回 `true`。

## 运行时不做什么

- 不发布遥测——这是产品的责任。
- 不直接管理 MQTT 重新连接——`CloudStateMachine` 处理这个。
- 不知道设备特定的配置参数。
