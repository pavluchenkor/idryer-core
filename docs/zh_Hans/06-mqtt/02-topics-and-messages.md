# MQTT 主题和消息

所有主题的形式为 `idryer/{serial}/{suffix}`，其中 `{serial}` 是设备序列号。

本文档描述由 `idryer-core` 中的 `MqttClient` 实现的主题和命令。完整的平台界面（所有设备类型的所有后端命令）在 `contracts/portal_backend_status.md` 中。

## 设备 → 后端

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

在设备首次上线时发布一次，收到 `ping` 命令时再发布一次。

有效负载由产品通过 `IProfile::buildInfoJson()` 定义。后端至少期望的字段：`hardwareVersion`、`firmwareVersion`、`timestamp`。

存储链接的示例：

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

由产品通过 `pub.publishTelemetry()` 发布。库不自动发布。

存储链接的示例（气候传感器）：

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

由产品在状态改变时通过 `pub.publishStatus()` 发布。有效负载由产品定义。

### config

```
idryer/{serial}/config    retained=false    on request
```

收到 `device.getConfig`（invoke）时发布或响应 `get_config` 时发布。通过 `pub.publishConfig()` 或 `pub.publishConfigRaw()` 调用。

对于大有效负载（> 16000 字节），分块发布：每个块包含 `tid`、`idx`、`total`、`last`、`d`。

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

通过 `pub.publishConfigDelta()` 的部分配置更新。后端期望一个 `d` 字段（一个包含更改的对象）。

### events

```
idryer/{serial}/events    retained=false    on event
```

由产品通过 `pub.publishEvent()` 发布。库不自动生成事件。

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

由 `LinkIntegrationsManager` 发布。包含活跃的集成连接状态。

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

当 TCP 连接断开时由代理自动设置。设备从不手动发布此主题。

## 后端 → 设备

设备订阅 `idryer/{serial}/commands/#`。

### commands/ping

```
idryer/{serial}/commands/ping
```

由 `IdryerRuntime` 直接处理——通过 `settimeofday()` 同步系统时间并重新发布信息。

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

产品动作的首选路径。库将命令传递给产品的 `CommandHandler`（推荐路径）。如果没有注册 `CommandHandler`，命令通过到内置 `ActionDispatcher`（回退）。

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

内置操作 `device.getConfig` 由运行时或产品处理程序处理——调用 `IProfile::getConfig()` 并发布结果。

### commands/set

```
idryer/{serial}/commands/set
```

设置单个配置参数。传递给产品的 `CommandHandler`（推荐路径）。回退——如果没有注册 `CommandHandler`，内置 `ActionDispatcher::handleSet()`。

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

对 Bambu 打印机上的 AMS 插槽应用耗材参数。由 `LinkIntegrationsManager` 处理。

### 其他平台命令

命令 `drying`、`storage`、`profile`、`stop`、`get_config`、`read_rfid`、`write_rfid` 等是完整 iDryer 平台接口的一部分。它们不由 `idryer-core` 直接处理；它们传递给产品的 `CommandHandler`。参考：`contracts/portal_backend_status.md`。

## 主题格式

```c
// 主题构造
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

后缀常数在 `mqtt/idryer_topics.h` 中定义：

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
