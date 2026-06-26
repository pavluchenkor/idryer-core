# MQTT topic 和消息

所有 topic 都采用 `idryer/{serial}/{suffix}` 形式，其中 `{serial}` 是设备序列号。

本文描述 `idryer-core` 中 `MqttClient` 实现的 topic 和命令。完整平台接口（所有设备类型的全部后端命令）位于 `contracts/portal_backend_status.md`。

## 设备 → 后端

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

设备第一次进入 Online 时发布一次；收到 `ping` 命令时再次发布。

Payload 由产品通过 `IProfile::buildInfoJson()` 定义。后端至少期望字段：`hardwareVersion`、`firmwareVersion`、`timestamp`。

Storage Link 示例：

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

### telemetry

```
idryer/{serial}/telemetry    retained=false    interval ~10 s
```

由产品通过 `pub.publishTelemetry()` 发布。库不会自动发布。

Storage Link 示例（气候传感器）：

```json
{
  "units": [
    {"unitId": "U1", "temperature": 23.5, "humidity": 47.2}
  ]
}
```

### status

```
idryer/{serial}/status    retained=true    published on change
```

由产品在状态变化时通过 `pub.publishStatus()` 发布。Payload 由产品定义。

### config

```
idryer/{serial}/config    retained=false    on request
```

收到 `device.getConfig`（invoke）或响应 `get_config` 时发布。通过 `pub.publishConfig()` 或 `pub.publishConfigRaw()` 调用。

对于大 payload（> 16000 字节），会分块发布：每个分块包含 `tid`、`idx`、`total`、`last`、`d`。

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

通过 `pub.publishConfigDelta()` 发布部分配置更新。后端期望 `d` 字段（包含变更的对象）。

### events

```
idryer/{serial}/events    retained=false    on event
```

由产品通过 `pub.publishEvent()` 发布。库不会自动生成事件。

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

由 `LinkIntegrationsManager` 发布。包含当前活动集成的连接状态。

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

TCP 连接意外断开时由 broker 自动设置。设备不会手动发布此 topic。

## 后端 → 设备

设备订阅 `idryer/{serial}/commands/#`。

### commands/ping

```
idryer/{serial}/commands/ping
```

由 `IdryerRuntime` 直接处理：通过 `settimeofday()` 同步系统时间，并重新发布 info。

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

产品动作的首选路径。库会把命令交给产品的 `CommandHandler`（推荐路径）。如果没有注册 `CommandHandler`，命令会落到内置的 `ActionDispatcher`（fallback）。

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

内置动作 `device.getConfig` 由 runtime 或产品 handler 处理：调用 `IProfile::getConfig()` 并发布结果。

### commands/set

```
idryer/{serial}/commands/set
```

设置单个配置参数。传给产品的 `CommandHandler`（推荐路径）。Fallback 是在没有注册 `CommandHandler` 时调用内置 `ActionDispatcher::handleSet()`。

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

集成管理。由 `LinkIntegrationsManager` 通过产品的 `CommandHandler` 处理。

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```

### commands/bambu_apply

```
idryer/{serial}/commands/bambu_apply
```

将耗材参数应用到 Bambu 打印机的 AMS 槽位。由 `LinkIntegrationsManager` 处理。

### 其他平台命令

`drying`、`storage`、`profile`、`stop`、`get_config`、`read_rfid`、`write_rfid` 等命令属于完整 iDryer 平台接口。它们不由 `idryer-core` 直接处理，而是传递给产品的 `CommandHandler`。参考：`contracts/portal_backend_status.md`。

## Topic 格式

```c
// Topic construction
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

Suffix 常量定义在 `mqtt/idryer_topics.h`：

```c
#define IDRYER_TOPIC_INFO               "info"
#define IDRYER_TOPIC_TELEMETRY          "telemetry"
#define IDRYER_TOPIC_STATUS             "status"
#define IDRYER_TOPIC_CONFIG             "config"
#define IDRYER_TOPIC_CONFIG_DELTA       "config/delta"
#define IDRYER_TOPIC_EVENTS             "events"
#define IDRYER_TOPIC_OFFLINE            "offline"
#define IDRYER_TOPIC_INTEGRATIONS_STATUS "integrations/status"
#define IDRYER_TOPIC_CMD_WILDCARD       "commands/#"
```
