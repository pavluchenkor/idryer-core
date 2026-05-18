# MqttClient

`MqttClient` 是设备的 MQTT 客户端。它包装 `PubSubClient`，管理连接并路由传入消息。所有主题都从设备序列号自动形成。

## 初始化

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

由 `CloudStateMachine` 在成功配置后调用。不立即连接——设置参数并配置 TLS。

参数：

- `serialNumber` — 设备序列号。用作 MQTT 客户端 ID 和用户名。
- `token` — 设备令牌。用作 MQTT 密码。

使用标志 `MQTT_USE_TLS=1` 构建时，客户端使用 Let's Encrypt 根 CA 配置 `WiFiClientSecure`（嵌入在 `root_ca.h` 中）。

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // 参见下面的"缓冲区大小"
mqttClient_.setKeepAlive(60);
```

## 缓冲区大小 {#buffer-size}

`PubSubClient` 默认使用 256 字节缓冲区——仅足以用于短消息。对于 iDryer 设备，这太小了：主要的"重型"有效负载是设备配置（菜单），发布到主题 `idryer/{serial}/config` 一次。

`MqttClient` 将缓冲区设置为 `MQTT_BUFFER_SIZE`，并限制大型配置的块大小为 `MQTT_CONFIG_CHUNK_SIZE`。两个常数都定义在 `lib/idryer-core/src/mqtt/mqtt_client.h` 中：

```cpp
#define MQTT_BUFFER_SIZE        16384  // PubSubClient 缓冲区
#define MQTT_CONFIG_CHUNK_SIZE  16000  // 配置块中的最大数据
```

它们之间的关系：

- `MQTT_BUFFER_SIZE`（16384 字节）— **一个 MQTT 消息** 的上限。任何 `publish*()` 调用的有效负载大于此将被 `PubSubClient` 丢弃而不发送。
- `MQTT_CONFIG_CHUNK_SIZE`（16000 字节）— 一个 `publishConfigRaw` 块内 `"d"`（数据部分）的最大大小。384 字节的余量为块信封预留：`{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}` 加上自动添加的 `timestamp` 字段。

### 为什么是 16384

该数字的选择不是出于美学，而是基于**预期的最大设备有效负载**，即设置/菜单传输：

- 存储链接和 Link/iHeater 配置（菜单）序列化为带转义的 JSON。当前菜单的完整快照适合约 10–14 KB。
- 到 16384 的余量覆盖菜单增长而无需拆分成块。
- 该值是 4 KB 的倍数——对于 ESP32 上的分配很方便。

如果您的产品有更大的配置（例如，有许多项或二进制值的扩展菜单），有两条路径可用：

1. **提高 `MQTT_BUFFER_SIZE`** — 通过 `platformio.ini` 中的 `build_flags` 覆盖：
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   记住 RAM 使用情况：`PubSubClient` 连续保持这个缓冲区。在 ESP32-C3（~400 KB 自由堆）上，32 KB 是可接受的，但进一步使用开始有风险。

2. **使用 `publishConfigRaw(json, length)`** — 它将有效负载拆分为 `MQTT_CONFIG_CHUNK_SIZE` 的块；后端通过 `tid` / `idx` / `total` / `last` 字段重新组装它们。这条路径对于来自 UART 上 RP2040 的任意长度配置是首选。

### 适用于产品发布

相同的 16384 字节限制适用于 `publishTelemetry`、`publishStatus`、`publishEvent`。实际上，遥测和事件要小得多（几百字节）；只有配置发布接近此限制。如果您的项目定期发布大有效负载（例如，测量数组转储），提前估计其大小或自己拆分。

## 连接

```cpp
bool MqttClient::connect();
```

执行：

1. 与代理的连接具有持久会话（`clean_session = false`）。持久会话是强制性的——没有它，在设备离线时到达的命令将丢失。
2. 在主题 `idryer/{serial}/offline` 上设置 LWT 消息（QoS 1，不保留）。
3. 订阅 `idryer/{serial}/commands/#`（QoS 1）。最多尝试 3 次；失败时，断开连接。

如果连接和订阅成功，返回 `true`。

## 循环

```cpp
void MqttClient::loop();
```

每次迭代调用。在断开连接时重新连接，然后调用 `PubSubClient::loop()` 以接收传入消息。

## 发布

如果 JSON 文档中不存在 `timestamp` 字段，所有发布方法都会添加它（ISO 8601 UTC）。

| 方法 | 主题 | 保留 |
|------|------|------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | 是 |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | 否 |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | 是 |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | 否 |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | 否 |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | 是 |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | 否 |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | 否 |

`publishConfigRaw` 如果大小超过 `MQTT_CONFIG_CHUNK_SIZE`（16000 字节），会自动将有效负载拆分成块。每个块包含字段 `tid`、`idx`、`total`、`last`、`d`。

!!! note
    `PubSubClient` 总是以 QoS 0 发布，不管主题设置。这是库的限制。

## 接收命令

主题 `idryer/{serial}/commands/{cmd}` 中的传入消息被解析为 JSON 并传递给注册的 `CommandCallback`：

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

`{cmd}` 部分从主题中提取并作为第一个参数传递。`IdryerRuntime` 在 `begin()` 中注册此回调。

## 助手方法

```cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bytes
static char* generateUuid(char* buffer);    // buffer >= 37 bytes
```

`generateUuid` 基于 `esp_random()` 生成 UUID v4。

## 限制

- 每个设备一个 `MqttClient` 实例（通过 `instance_` 的单例）。
- 单个 JSON 消息的最大大小 — `MQTT_BUFFER_SIZE`（默认 16384 字节）。为最重的设备有效负载调整大小——通常是序列化的配置（菜单）。对于更大的配置，通过 `build_flags` 提高常数或使用具有自动块拆分的 `publishConfigRaw`。参见 [缓冲区大小](#buffer-size)。
- TLS 由构建标志 `MQTT_USE_TLS` 启用。
