# Mapa de módulos

La biblioteca se incluye a través de uno de los tres encabezados:

```cpp
#include <idryer_core.h>         // núcleo requerido
#include <idryer_uart.h>         // opcional: puente UART
#include <idryer_integrations.h> // opcional: HA / Bambu / Moonraker
```

## Tabla de módulos

| Módulo | Encabezado | Propósito | Requerido |
|--------|---------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | Constantes base (`IDRYER_MAX_*`, tiempos de espera) y estructuras `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Siempre |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Abstracción de tiempo y registro (`ITime`, `ILogger`, macros `HAL_LOG_*`). Implementaciones de Arduino: `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Siempre |
| **device/interfaces** | `device/interfaces/` | Interfaces de plataforma: `IWifiManager`, `IHttpClient`, `ICredentialStore` | Siempre |
| **platform/arduino** | `platform/arduino/` | Implementaciones de Arduino de interfaces: `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Siempre (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | Cliente MQTT basado en `PubSubClient`: conexión, suscripción a `commands/#`, publicación de todos los temas | Siempre |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Máquina de estados de conexión en la nube (WiFi → Provision → Claim → MQTT). API HTTP para aprovisionamiento. Despachador de comandos `invoke`/`set` | Siempre |
| **profiles** | `profiles/IProfile.h` | Interfaz `IProfile` — contrato entre la biblioteca y el producto | Siempre |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — coordinador de nivel superior: conecta `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` en un `begin()`/`loop()` unificado | Siempre |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Puente UART bidireccional para dispositivos dual MCU (ESP32 + RP2040). Protocolo de trama, CRC-16, ACK/retry | **Opcional** (dispositivos dual MCU) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — ensamblaje y transmisión de configuración JSON fragmentada sobre UART | **Opcional** (con uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + tres clientes (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Almacenamiento de configuración de integración en NVS | **Opcional** |

## Dependencias entre módulos

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` y `LinkIntegrationsManager` se conectan a través de `CommandHandler` del producto (`setCommandHandler()`) y no son dependencias de base.
