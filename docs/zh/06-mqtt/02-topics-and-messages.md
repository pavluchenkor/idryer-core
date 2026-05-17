# MQTT 主題和消息

All topics have the form `idryer/{serial}/{suffix}`, where `{serial}` is the device serial number.

This document describes the topics and commands implemented by `MqttClient` from `idryer-core`. The full platform interface (all backend commands for all device types) is in `contracts/portal_backend_status.md`.

## 設備 → 後端

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

Published once when the device first goes Online, and again on receiving a `ping` command.

The payload is defined by the product via `IProfile::buildInfoJson()`. Fields expected by the backend at a minimum: `hardwareVersion`, `firmwareVersion`, `timestamp`.

Example for Storage Link:

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

Published by the product via `pub.publishTelemetry()`. The library does not publish automatically.

Example for Storage Link (climate sensor):

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

Published by the product on state change via `pub.publishStatus()`. Payload is defined by the product.

### config

```
idryer/{serial}/config    retained=false    on request
```

Published on receipt of `device.getConfig` (invoke) or in response to `get_config`. Called via `pub.publishConfig()` or `pub.publishConfigRaw()`.

For large payloads (> 16000 bytes), published in chunks: each chunk contains `tid`, `idx`, `total`, `last`, `d`.

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

Partial config update via `pub.publishConfigDelta()`. The backend expects a `d` field (an object with the changes).

### events

```
idryer/{serial}/events    retained=false    on event
```

Published by the product via `pub.publishEvent()`. The library does not generate events automatically.

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

Published by `LinkIntegrationsManager`. Contains the active integration connection state.

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

Set by the broker automatically when the TCP connection drops. The device never publishes this topic manually.

## 後端 → 設備

The device subscribes to `idryer/{serial}/commands/#`.

### commands/ping

```
idryer/{serial}/commands/ping
```

Handled directly by `IdryerRuntime` — syncs system time via `settimeofday()` and re-publishes info.

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

Preferred path for product actions. The library passes the command to the product's `CommandHandler` (recommended path). If no `CommandHandler` is registered, the command falls through to the built-in `ActionDispatcher` (fallback).

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

The built-in action `device.getConfig` is handled by the runtime or the product handler — calls `IProfile::getConfig()` and publishes the result.

### commands/set

```
idryer/{serial}/commands/set
```

Sets a single configuration parameter. Passed to the product's `CommandHandler` (recommended path). Fallback — built-in `ActionDispatcher::handleSet()` if no `CommandHandler` is registered.

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

Integration management. Handled by `LinkIntegrationsManager` via the product's `CommandHandler`.

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```

### commands/bambu_apply

```
idryer/{serial}/commands/bambu_apply
```

Apply filament parameters to an AMS slot on a Bambu printer. Handled by `LinkIntegrationsManager`.

### 其他平台命令

Commands `drying`, `storage`, `profile`, `stop`, `get_config`, `read_rfid`, `write_rfid`, and others are part of the full iDryer platform interface. They are not handled by `idryer-core` directly; they are delivered to the product's `CommandHandler`. Reference: `contracts/portal_backend_status.md`.

## Topic format

```c
// Topic construction
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

Suffix constants are defined in `mqtt/idryer_topics.h`:

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
