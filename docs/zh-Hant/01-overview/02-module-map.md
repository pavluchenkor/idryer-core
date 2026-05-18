# 模塊地圖

該庫通過三個標題之一包含：

```cpp
#include <idryer_core.h>         // 必需核心
#include <idryer_uart.h>         // 可選：UART 橋
#include <idryer_integrations.h> // 可選：HA / Bambu / Moonraker
```

## 模塊表

| 模塊 | 標題 | 目的 | 必需 |
|--------|--------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | 基本常量（`IDRYER_MAX_*`、超時）和結構體 `DeviceIdentity`、`WifiCredentials`、`CommandCallback` | 總是 |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | 時間和日誌記錄抽象（`ITime`、`ILogger`、`HAL_LOG_*` 宏）。Arduino 實現：`ArduinoTime`、`ArduinoLogger`、`ArduinoSerial` | 總是 |
| **device/interfaces** | `device/interfaces/` | 平台接口：`IWifiManager`、`IHttpClient`、`ICredentialStore` | 總是 |
| **platform/arduino** | `platform/arduino/` | 接口的 Arduino 實現：`ArduinoWifiManager`、`ArduinoCredentialStore`、`ArduinoHttpClient`、`ArduinoWifiStore` | 總是（ESP32/Arduino） |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | 基於 `PubSubClient` 的 MQTT 客戶端：連接、訂閱 `commands/#`、發佈所有主題 | 總是 |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | 雲連接狀態機（WiFi → Provision → Claim → MQTT）。配置的 HTTP API。`invoke`/`set` 命令調度程序 | 總是 |
| **profiles** | `profiles/IProfile.h` | `IProfile` 接口 — 庫和產品之間的契約 | 總是 |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — 頂級協調器：將 `CloudStateMachine`、`ActionDispatcher`、`IProfile`、`MqttClient` 連接到統一的 `begin()`/`loop()` | 總是 |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | 雙向 UART 橋，用於雙 MCU 設備（ESP32 + RP2040）。幀協議、CRC-16、ACK/重試 | **可選**（雙 MCU 設備） |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — 通過 UART 組裝和傳輸分段 JSON 配置 | **可選**（帶 uart） |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + 三個客戶端（Bambu LAN MQTT、HA MQTT、Moonraker WebSocket）。NVS 中的集成配置存儲 | **可選** |

## 模塊依賴關係

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` 和 `LinkIntegrationsManager` 通過產品的 `CommandHandler`（`setCommandHandler()`）連接，不是核心依賴項。
