# Carte des modules

La bibliothèque est incluse via l'un des trois en-têtes :

```cpp
#include <idryer_core.h>         // core requis
#include <idryer_uart.h>         // optionnel : pont UART
#include <idryer_integrations.h> // optionnel : HA / Bambu / Moonraker
```

## Tableau des modules

| Module | En-tête | Objectif | Requis |
|--------|---------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | Constantes de base (`IDRYER_MAX_*`, délais) et structures `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Toujours |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Abstraction du temps et de la journalisation (`ITime`, `ILogger`, macros `HAL_LOG_*`). Implémentations Arduino : `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Toujours |
| **device/interfaces** | `device/interfaces/` | Interfaces de plate-forme : `IWifiManager`, `IHttpClient`, `ICredentialStore` | Toujours |
| **platform/arduino** | `platform/arduino/` | Implémentations Arduino des interfaces : `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Toujours (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | Client MQTT basé sur `PubSubClient` : connexion, abonnement à `commands/#`, publication de tous les sujets | Toujours |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Machine à états de connexion cloud (WiFi → Provision → Claim → MQTT). API HTTP pour l'approvisionnement. Répartiteur de commandes `invoke`/`set` | Toujours |
| **profiles** | `profiles/IProfile.h` | Interface `IProfile` — contrat entre la bibliothèque et le produit | Toujours |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — coordinateur de haut niveau : connecte `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` dans un `begin()`/`loop()` unifié | Toujours |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Pont UART bidirectionnel pour appareils double MCU (ESP32 + RP2040). Protocole de trame, CRC-16, ACK/retry | **Optionnel** (appareils double MCU) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — assemblage et transmission d'une configuration JSON fragmentée sur UART | **Optionnel** (avec uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + trois clients (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Stockage de configuration d'intégration dans NVS | **Optionnel** |

## Dépendances entre modules

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` et `LinkIntegrationsManager` sont connectés via `CommandHandler` du produit (`setCommandHandler()`) et ne sont pas des dépendances de base.
