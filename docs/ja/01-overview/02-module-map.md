# モジュールマップ

ライブラリは3つのヘッダーのいずれかを介して含まれます：

```cpp
#include <idryer_core.h>         // 必須コア
#include <idryer_uart.h>         // オプション：UARTブリッジ
#include <idryer_integrations.h> // オプション：HA / Bambu / Moonraker
```

## モジュール表

| モジュール | ヘッダー | 目的 | 必須 |
|--------|--------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | 基本定数（`IDRYER_MAX_*`、タイムアウト）と構造体 `DeviceIdentity`、`WifiCredentials`、`CommandCallback` | 常に |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | 時間およびロギング抽象化（`ITime`、`ILogger`、`HAL_LOG_*` マクロ）。Arduino実装：`ArduinoTime`、`ArduinoLogger`、`ArduinoSerial` | 常に |
| **device/interfaces** | `device/interfaces/` | プラットフォームインターフェース：`IWifiManager`、`IHttpClient`、`ICredentialStore` | 常に |
| **platform/arduino** | `platform/arduino/` | インターフェースのArduino実装：`ArduinoWifiManager`、`ArduinoCredentialStore`、`ArduinoHttpClient`、`ArduinoWifiStore` | 常に（ESP32/Arduino） |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | `PubSubClient`ベースのMQTTクライアント：接続、`commands/#`への購読、すべてのトピックの発行 | 常に |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | クラウド接続状態マシン（WiFi → Provision → Claim → MQTT）。プロビジョニング用HTTP API。`invoke`/`set` コマンドディスパッチャー | 常に |
| **profiles** | `profiles/IProfile.h` | `IProfile` インターフェース — ライブラリと製品間の契約 | 常に |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — トップレベルコーディネーター：`CloudStateMachine`、`ActionDispatcher`、`IProfile`、`MqttClient`を統一された`begin()`/`loop()`に接続 | 常に |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | デュアルMCUデバイス（ESP32 + RP2040）用の双方向UARTブリッジ。フレームプロトコル、CRC-16、ACK/再試行 | **オプション**（デュアルMCUデバイス） |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — UART経由の断片化されたJSON設定の組み立てと送信 | **オプション**（uart使用時） |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + 3つのクライアント（Bambu LAN MQTT、HA MQTT、Moonraker WebSocket）。NVSの統合設定ストレージ | **オプション** |

## モジュール依存関係

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` と `LinkIntegrationsManager` は製品の `CommandHandler`（`setCommandHandler()`）を介して配線され、コア依存関係ではありません。
