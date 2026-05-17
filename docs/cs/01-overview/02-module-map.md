# Mapa modulů

Knihovna je zahrnuta jedním ze tří hlaviček:

```cpp
#include <idryer_core.h>         // vyžadované jádro
#include <idryer_uart.h>         // volitelné: UART most
#include <idryer_integrations.h> // volitelné: HA / Bambu / Moonraker
```

## Tabulka modulů

| Modul | Hlavička | Účel | Vyžadováno |
|-------|----------|------|-----------|
| **core** | `core/config.h`, `core/types.h` | Základní konstanty (`IDRYER_MAX_*`, timeouty) a struktury `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Vždy |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Abstrakce času a loggování (`ITime`, `ILogger`, `HAL_LOG_*` makra). Implementace Arduino: `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Vždy |
| **device/interfaces** | `device/interfaces/` | Rozhraní platformy: `IWifiManager`, `IHttpClient`, `ICredentialStore` | Vždy |
| **platform/arduino** | `platform/arduino/` | Implementace Arduino rozhraní: `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Vždy (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | MQTT klient na základě `PubSubClient`: připojení, přihlášení k `commands/#`, publikování všech témat | Vždy |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Stavový automata připojení v cloudu (WiFi → Provision → Claim → MQTT). HTTP API pro zřizování. Dispečer příkazů `invoke`/`set` | Vždy |
| **profiles** | `profiles/IProfile.h` | Rozhraní `IProfile` — smlouva mezi knihovnou a produktem | Vždy |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — koordinátor nejvyšší úrovně: spojuje `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` do jednotného `begin()`/`loop()` | Vždy |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Obousměrný UART most pro zařízení se dvěma MCU (ESP32 + RP2040). Frame protokol, CRC-16, ACK/retry | **Volitelné** (zařízení se dvěma MCU) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — montáž a přenos fragmentované konfigurace JSON přes UART | **Volitelné** (s uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + tři klienti (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Úložiště konfigurace integrace v NVS | **Volitelné** |

## Závislosti modulů

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` a `LinkIntegrationsManager` jsou připojeny prostřednictvím `CommandHandler` produktu (`setCommandHandler()`) a nejsou základními závislostmi.
