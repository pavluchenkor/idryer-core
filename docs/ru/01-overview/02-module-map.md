# Карта модулей

Библиотека включается через один из трёх заголовков:

```cpp
#include <idryer_core.h>         // обязательное ядро
#include <idryer_uart.h>         // опционально: UART-бридж
#include <idryer_integrations.h> // опционально: HA / Bambu / Moonraker
```

## Таблица модулей

| Модуль | Заголовок | Назначение | Обязательность |
|--------|-----------|------------|----------------|
| **core** | `core/config.h`, `core/types.h` | Базовые константы (`IDRYER_MAX_*`, таймауты) и структуры `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Всегда |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Абстракция времени и логирования (`ITime`, `ILogger`, `HAL_LOG_*` макросы). Arduino-реализации: `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Всегда |
| **device/interfaces** | `device/interfaces/` | Интерфейсы платформы: `IWifiManager`, `IHttpClient`, `ICredentialStore` | Всегда |
| **platform/arduino** | `platform/arduino/` | Arduino-реализации интерфейсов: `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Всегда (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | MQTT-клиент на базе `PubSubClient`: подключение, подписка на `commands/#`, публикация всех топиков | Всегда |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Стейт-машина облачного подключения (WiFi → Provision → Claim → MQTT). HTTP API для provisioning. Диспетчер команд `invoke`/`set` | Всегда |
| **profiles** | `profiles/IProfile.h` | Интерфейс `IProfile` — контракт между библиотекой и продуктом | Всегда |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — верхний координатор: связывает `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` в единый `begin()`/`loop()` | Всегда |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Бидиректиональный UART-бридж для двухпроцессорных устройств (ESP32 + RP2040). Фреймовый протокол, CRC-16, ACK/retry | **Опционально** (двухпроцессорные устройства) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — сборка и отправка фрагментированного JSON-конфига по UART | **Опционально** (с uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + три клиента (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Хранение конфигурации интеграций в NVS | **Опционально** |

## Зависимости между модулями

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` и `LinkIntegrationsManager` подключаются через продуктовый `CommandHandler` (`setCommandHandler()`) и не являются зависимостями ядра.
