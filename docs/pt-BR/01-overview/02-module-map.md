# Mapa de módulos

A biblioteca é incluída por um dos três cabeçalhos:

```cpp
#include <idryer_core.h>         // núcleo obrigatório
#include <idryer_uart.h>         // opcional: ponte UART
#include <idryer_integrations.h> // opcional: HA / Bambu / Moonraker
```

## Tabela de módulos

| Módulo | Cabeçalho | Propósito | Obrigatório |
|--------|-----------|-----------|-----------|
| **core** | `core/config.h`, `core/types.h` | Constantes base (`IDRYER_MAX_*`, tempos limite) e estruturas `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Sempre |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Abstração de tempo e logging (`ITime`, `ILogger`, macros `HAL_LOG_*`). Implementações Arduino: `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Sempre |
| **device/interfaces** | `device/interfaces/` | Interfaces de plataforma: `IWifiManager`, `IHttpClient`, `ICredentialStore` | Sempre |
| **platform/arduino** | `platform/arduino/` | Implementações Arduino de interfaces: `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Sempre (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | Cliente MQTT baseado em `PubSubClient`: conexão, subscrição em `commands/#`, publicação de todos os tópicos | Sempre |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Máquina de estados de conexão em nuvem (WiFi → Provision → Claim → MQTT). API HTTP para provisionamento. Dispatcher de comandos `invoke`/`set` | Sempre |
| **profiles** | `profiles/IProfile.h` | Interface `IProfile` — contrato entre a biblioteca e o produto | Sempre |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — coordenador de nível superior: conecta `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` em um único `begin()`/`loop()` | Sempre |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Ponte UART bidirecional para dispositivos com dois MCU (ESP32 + RP2040). Protocolo de quadro, CRC-16, ACK/retry | **Opcional** (dispositivos com dois MCU) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — montagem e transmissão de configuração JSON fragmentada sobre UART | **Opcional** (com uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + três clientes (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Armazenamento de configuração de integração em NVS | **Opcional** |

## Dependências de módulos

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` e `LinkIntegrationsManager` são conectados através do `CommandHandler` do produto (`setCommandHandler()`) e não são dependências principais.
