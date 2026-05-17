# 模块地图

该库通过三个标题之一包含：

```cpp
#include <idryer_core.h>         // 必需核心
#include <idryer_uart.h>         // 可选：UART 桥
#include <idryer_integrations.h> // 可选：HA / Bambu / Moonraker
```

## 模块表

| 模块 | 标题 | 目的 | 必需 |
|--------|--------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | 基本常量（`IDRYER_MAX_*`、超时）和结构体 `DeviceIdentity`、`WifiCredentials`、`CommandCallback` | 总是 |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | 时间和日志记录抽象（`ITime`、`ILogger`、`HAL_LOG_*` 宏）。Arduino 实现：`ArduinoTime`、`ArduinoLogger`、`ArduinoSerial` | 总是 |
| **device/interfaces** | `device/interfaces/` | 平台接口：`IWifiManager`、`IHttpClient`、`ICredentialStore` | 总是 |
| **platform/arduino** | `platform/arduino/` | 接口的 Arduino 实现：`ArduinoWifiManager`、`ArduinoCredentialStore`、`ArduinoHttpClient`、`ArduinoWifiStore` | 总是（ESP32/Arduino） |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | 基于 `PubSubClient` 的 MQTT 客户端：连接、订阅 `commands/#`、发布所有主题 | 总是 |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | 云连接状态机（WiFi → Provision → Claim → MQTT）。配置的 HTTP API。`invoke`/`set` 命令调度程序 | 总是 |
| **profiles** | `profiles/IProfile.h` | `IProfile` 接口 — 库和产品之间的契约 | 总是 |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — 顶级协调器：将 `CloudStateMachine`、`ActionDispatcher`、`IProfile`、`MqttClient` 连接到统一的 `begin()`/`loop()` | 总是 |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | 双向 UART 桥，用于双 MCU 设备（ESP32 + RP2040）。帧协议、CRC-16、ACK/重试 | **可选**（双 MCU 设备） |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — 通过 UART 组装和传输分段 JSON 配置 | **可选**（带 uart） |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + 三个客户端（Bambu LAN MQTT、HA MQTT、Moonraker WebSocket）。NVS 中的集成配置存储 | **可选** |

## 模块依赖关系

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` 和 `LinkIntegrationsManager` 通过产品的 `CommandHandler`（`setCommandHandler()`）连接，不是核心依赖项。
