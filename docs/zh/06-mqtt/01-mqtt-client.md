# MqttClient

`MqttClient` 是设备的 MQTT 客户端。它封装 `PubSubClient`、管理连接并路由传入消息。所有 topic 都会根据设备序列号自动生成。

## 初始化

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

由 `CloudStateMachine` 在 provisioning 成功后调用。它不会立即连接，只会设置参数并配置 TLS。

参数：

- `serialNumber` — 设备序列号。用作 MQTT client ID 和用户名。
- `token` — 设备 token。用作 MQTT 密码。

使用 `MQTT_USE_TLS=1` 构建时，客户端会用 Let's Encrypt 根 CA（嵌入在 `root_ca.h` 中）配置 `WiFiClientSecure`。

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // see "Buffer size" below
mqttClient_.setKeepAlive(60);
```

## 缓冲区大小 {#buffer-size}

`PubSubClient` 默认使用 256 字节缓冲区，只够短消息使用。对于 iDryer 设备这太小了：主要的“大” payload 是设备配置（菜单），会一次性发布到 `idryer/{serial}/config` topic。

`MqttClient` 将缓冲区设为 `MQTT_BUFFER_SIZE`，并把大配置的分块大小限制为 `MQTT_CONFIG_CHUNK_SIZE`。两个常量都定义在 `lib/idryer-core/src/mqtt/mqtt_client.h`：

```cpp
#define MQTT_BUFFER_SIZE        16384  // PubSubClient buffer
#define MQTT_CONFIG_CHUNK_SIZE  16000  // maximum data in one config chunk
```

两者关系：

- `MQTT_BUFFER_SIZE`（16384 字节）— **单条 MQTT 消息**的上限。任何 `publish*()` 调用如果 payload 超过此大小，都会被 `PubSubClient` 丢弃，不会发送。
- `MQTT_CONFIG_CHUNK_SIZE`（16000 字节）— 单个 `publishConfigRaw` 分块中 `"d"`（数据部分）的最大大小。剩余 384 字节留给分块封套：`{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}`，以及自动添加的 `timestamp` 字段。

### 为什么是 16384

这个数字不是为了好看，而是来自**预期最大的设备 payload**，也就是设置/菜单传输：

- Storage Link 和 Link/iHeater 的配置（菜单）序列化为带转义的 JSON。当前菜单的完整快照约为 10–14 KB。
- 到 16384 的余量可以覆盖菜单增长，而不必拆成分块。
- 该值是 4 KB 的倍数，便于 ESP32 上的内存分配。

如果你的产品有更大的配置（例如包含很多项或二进制值的扩展菜单），有两种路径：

1. **提高 `MQTT_BUFFER_SIZE`** — 通过 `platformio.ini` 中的 `build_flags` 覆盖：
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   注意 RAM 占用：`PubSubClient` 会持续持有这个缓冲区。ESP32-C3（约 400 KB 可用 heap）上 32 KB 可以接受，但继续增大就会带来风险。

2. **使用 `publishConfigRaw(json, length)`** — 它会把 payload 按 `MQTT_CONFIG_CHUNK_SIZE` 拆分；后端根据 `tid` / `idx` / `total` / `last` 字段重新组装。对于从 RP2040 经 UART 以任意长度片段传来的配置，这是首选路径。

### 同样适用于产品发布

同样的 16384 字节上限也适用于 `publishTelemetry`、`publishStatus`、`publishEvent`。实际中遥测和事件通常小得多（几百字节）；只有配置发布会接近这个限制。如果项目会周期性发布大 payload（例如测量数组 dump），请提前估算大小或自行拆分。

## 连接

```cpp
bool MqttClient::connect();
```

执行以下操作：

1. 使用持久会话连接到 broker（`clean_session = false`）。持久会话是必须的；否则设备离线期间到达的命令会丢失。
2. 在 `idryer/{serial}/offline` topic 上设置 LWT 消息（QoS 1，非 retained）。
3. 订阅 `idryer/{serial}/commands/#`（QoS 1）。最多尝试 3 次；失败时断开连接。

如果连接和订阅成功，则返回 `true`。

## 循环

```cpp
void MqttClient::loop();
```

每次迭代调用。断线时重连，然后调用 `PubSubClient::loop()` 接收传入消息。

## 发布

如果文档中尚未包含 `timestamp` 字段，所有发布方法都会添加 ISO 8601 UTC 格式的 `timestamp`。

| 方法 | Topic | Retained |
|--------|-------|----------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | yes |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | no |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | yes |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | no |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | no |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | yes |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | no |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | no |

如果 payload 大小超过 `MQTT_CONFIG_CHUNK_SIZE`（16000 字节），`publishConfigRaw` 会自动分块。每个分块包含 `tid`、`idx`、`total`、`last`、`d` 字段。

!!! note
    无论 topic 设置如何，`PubSubClient` 总是以 QoS 0 发布。这是库的限制。

## 接收命令

`idryer/{serial}/commands/{cmd}` topic 中的传入消息会被解析为 JSON，并传给已注册的 `CommandCallback`：

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

`{cmd}` 部分会从 topic 中提取出来，并作为第一个参数传入。`IdryerRuntime` 在 `begin()` 中注册这个 callback。

## 辅助方法

```cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bytes
static char* generateUuid(char* buffer);    // buffer >= 37 bytes
```

`generateUuid` 会基于 `esp_random()` 生成 UUID v4。

## 限制

- 每个设备一个 `MqttClient` 实例（通过 `instance_` 实现 singleton）。
- 单条 JSON 消息最大大小为 `MQTT_BUFFER_SIZE`（默认 16384 字节）。该值按最重的设备 payload 设置，通常是序列化后的配置（菜单）。更大的配置请通过 `build_flags` 提高常量，或使用带自动分块的 `publishConfigRaw`。见 [缓冲区大小](#buffer-size)。
- TLS 由构建标志 `MQTT_USE_TLS` 启用。
