# MqttClient

`MqttClient` is the device's MQTT client. It wraps `PubSubClient`, manages the connection, and routes incoming messages. All topics are formed from the device serial number automatically.

## 初始化

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

Called by `CloudStateMachine` after successful provisioning. Does not connect immediately — sets parameters and configures TLS.

Parameters:

- `serialNumber` — device serial number. Used as the MQTT client ID and username.
- `token` — device token. Used as the MQTT password.

When built with the flag `MQTT_USE_TLS=1`, the client configures `WiFiClientSecure` with the Let's Encrypt root CA (embedded in `root_ca.h`).

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // see "Buffer size" below
mqttClient_.setKeepAlive(60);
```

## Buffer size {#buffer-size}

`PubSubClient` uses a 256-byte buffer by default — sufficient only for short messages. For iDryer devices this is too small: the main "heavy" payload is the device configuration (menu), which is published to the topic `idryer/{serial}/config` in one shot.

`MqttClient` sets the buffer to `MQTT_BUFFER_SIZE` and limits the chunk size for large configs to `MQTT_CONFIG_CHUNK_SIZE`. Both constants are defined in `lib/idryer-core/src/mqtt/mqtt_client.h`:

```cpp
#define MQTT_BUFFER_SIZE        16384  // PubSubClient buffer
#define MQTT_CONFIG_CHUNK_SIZE  16000  // maximum data in one config chunk
```

Relationship between them:

- `MQTT_BUFFER_SIZE` (16384 bytes) — upper limit for **one MQTT message**. Any `publish*()` call with a payload larger than this will be dropped by `PubSubClient` without sending.
- `MQTT_CONFIG_CHUNK_SIZE` (16000 bytes) — maximum size of `"d"` (the data portion) inside one `publishConfigRaw` chunk. The 384-byte margin is reserved for the chunk envelope: `{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}` plus the automatically added `timestamp` field.

### Why 16384

The number was chosen not for aesthetics but from the **maximum expected device payload**, which is the settings/menu transfer:

- Storage Link and Link/iHeater config (menu) serializes as JSON with escaping. A full snapshot of the current menu fits in ~10–14 KB.
- The margin to 16384 covers menu growth without having to split into chunks.
- The value is a multiple of 4 KB — convenient for allocation on ESP32.

If your product has a larger config (e.g., an extended menu with many items or binary values), two paths are available:

1. **Raise `MQTT_BUFFER_SIZE`** — override via `build_flags` in `platformio.ini`:
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   Keep RAM usage in mind: `PubSubClient` holds this buffer continuously. On ESP32-C3 (~400 KB free heap) 32 KB is acceptable, but going further starts carrying risks.

2. **Use `publishConfigRaw(json, length)`** — it splits the payload into chunks of `MQTT_CONFIG_CHUNK_SIZE`; the backend reassembles them by the `tid` / `idx` / `total` / `last` fields. This path is preferred for configs coming from RP2040 over UART in pieces of arbitrary length.

### Applies to product publications

The same 16384-byte ceiling applies to `publishTelemetry`, `publishStatus`, `publishEvent`. In practice, telemetry and events are much smaller (hundreds of bytes); only config publications approach this limit. If your project periodically publishes a large payload (e.g., a measurement array dump), estimate its size upfront or split it yourself.

## 連接

```cpp
bool MqttClient::connect();
```

Performs:

1. Connection to the broker with persistent session (`clean_session = false`). Persistent session is mandatory — without it commands arriving while the device is offline are lost.
2. Sets the LWT message on topic `idryer/{serial}/offline` (QoS 1, not retained).
3. Subscribes to `idryer/{serial}/commands/#` (QoS 1). Makes up to 3 attempts; on failure, disconnects.

Returns `true` if connection and subscription succeeded.

## 循環

```cpp
void MqttClient::loop();
```

Called every iteration. Reconnects on disconnect, then calls `PubSubClient::loop()` to receive incoming messages.

## 發佈

All publish methods add a `timestamp` field (ISO 8601 UTC) if it is not already present in the document.

| Method | Topic | Retained |
|--------|-------|----------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | yes |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | no |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | yes |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | no |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | no |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | yes |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | no |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | no |

`publishConfigRaw` automatically splits the payload into chunks if the size exceeds `MQTT_CONFIG_CHUNK_SIZE` (16000 bytes). Each chunk contains the fields `tid`, `idx`, `total`, `last`, `d`.

!!! note
    `PubSubClient` always publishes at QoS 0, regardless of topic settings. This is a library limitation.

## 接收命令

Incoming messages in topic `idryer/{serial}/commands/{cmd}` are parsed as JSON and passed to the registered `CommandCallback`:

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

The `{cmd}` part is extracted from the topic and passed as the first argument. `IdryerRuntime` registers this callback in `begin()`.

## 幫助方法

```cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bytes
static char* generateUuid(char* buffer);    // buffer >= 37 bytes
```

`generateUuid` generates a UUID v4 based on `esp_random()`.

## 限制

- One `MqttClient` instance per device (singleton via `instance_`).
- Maximum size of a single JSON message — `MQTT_BUFFER_SIZE` (default 16384 bytes). Sized for the heaviest device payload — typically the serialized config (menu). For larger configs raise the constant via `build_flags` or use `publishConfigRaw` with automatic chunk splitting. See [Buffer size](#buffer-size).
- TLS is enabled by the build flag `MQTT_USE_TLS`.
