# Module map

The library is included via one of three headers:

```cpp
#include <idryer_core.h>         // required core
#include <idryer_uart.h>         // optional: UART bridge
#include <idryer_integrations.h> // optional: HA / Bambu / Moonraker
```

## Module table

| Module | Header | Purpose | Required |
|--------|--------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | Base constants (`IDRYER_MAX_*`, timeouts) and structures `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Always |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Time and logging abstraction (`ITime`, `ILogger`, `HAL_LOG_*` macros). Arduino implementations: `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Always |
| **device/interfaces** | `device/interfaces/` | Platform interfaces: `IWifiManager`, `IHttpClient`, `ICredentialStore` | Always |
| **platform/arduino** | `platform/arduino/` | Arduino implementations of interfaces: `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Always (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | MQTT client based on `PubSubClient`: connection, subscription to `commands/#`, publishing all topics | Always |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Cloud connection state machine (WiFi → Provision → Claim → MQTT). HTTP API for provisioning. `invoke`/`set` command dispatcher | Always |
| **profiles** | `profiles/IProfile.h` | `IProfile` interface — contract between the library and the product | Always |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — top-level coordinator: connects `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` into a unified `begin()`/`loop()` | Always |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Bidirectional UART bridge for dual-MCU devices (ESP32 + RP2040). Frame protocol, CRC-16, ACK/retry | **Optional** (dual-MCU devices) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — assembly and transmission of a fragmented JSON config over UART | **Optional** (with uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + three clients (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Integration configuration storage in NVS | **Optional** |

## Module dependencies

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` and `LinkIntegrationsManager` are wired in through the product's `CommandHandler` (`setCommandHandler()`) and are not core dependencies.
