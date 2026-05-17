# Modülübersicht

Die Bibliothek wird durch einen von drei Headern eingebunden:

```cpp
#include <idryer_core.h>         // erforderlicher Kern
#include <idryer_uart.h>         // optional: UART-Brücke
#include <idryer_integrations.h> // optional: HA / Bambu / Moonraker
```

## Modultabelle

| Modul | Header | Zweck | Erforderlich |
|--------|--------|---------|----------|
| **core** | `core/config.h`, `core/types.h` | Basis-Konstanten (`IDRYER_MAX_*`, Timeouts) und Strukturen `DeviceIdentity`, `WifiCredentials`, `CommandCallback` | Immer |
| **hal** | `hal/hal_types.h`, `hal/hal_arduino.h` | Zeit- und Logging-Abstraktion (`ITime`, `ILogger`, `HAL_LOG_*` Makros). Arduino-Implementierungen: `ArduinoTime`, `ArduinoLogger`, `ArduinoSerial` | Immer |
| **device/interfaces** | `device/interfaces/` | Plattform-Interfaces: `IWifiManager`, `IHttpClient`, `ICredentialStore` | Immer |
| **platform/arduino** | `platform/arduino/` | Arduino-Implementierungen von Interfaces: `ArduinoWifiManager`, `ArduinoCredentialStore`, `ArduinoHttpClient`, `ArduinoWifiStore` | Immer (ESP32/Arduino) |
| **mqtt** | `mqtt/mqtt_client.h`, `mqtt/idryer_topics.h` | MQTT-Client basierend auf `PubSubClient`: Verbindung, Abonnement von `commands/#`, Veröffentlichung aller Topics | Immer |
| **cloud** | `cloud/cloud_state_machine.h`, `cloud/http_api.h`, `cloud/action_dispatcher.h` | Cloud-Verbindungs-Zustandsmaschine (WiFi → Provision → Claim → MQTT). HTTP-API für Bereitstellung. `invoke`/`set` Befehls-Dispatcher | Immer |
| **profiles** | `profiles/IProfile.h` | `IProfile` Interface — Vertrag zwischen Bibliothek und Produkt | Immer |
| **runtime** | `runtime/idryer_runtime.h` | `IdryerRuntime` — Top-Level-Koordinator: verbindet `CloudStateMachine`, `ActionDispatcher`, `IProfile`, `MqttClient` in ein vereinheitlichtes `begin()`/`loop()` | Immer |
| **uart** | `uart/uart_bridge.h`, `uart/uart_protocol.h` | Bidirektionale UART-Brücke für Dual-MCU-Geräte (ESP32 + RP2040). Frame-Protokoll, CRC-16, ACK/Retry | **Optional** (Dual-MCU-Geräte) |
| **config** | `config/config_manager.h` | `ConfigReceiver`/`ConfigSender` — Zusammenstellung und Übertragung einer fragmentierten JSON-Konfiguration über UART | **Optional** (mit uart) |
| **integrations** | `integrations/common/`, `integrations/bambu/`, `integrations/home_assistant/`, `integrations/moonraker/` | `LinkIntegrationsManager` + drei Clients (Bambu LAN MQTT, HA MQTT, Moonraker WebSocket). Integrationskonfiguration-Speicherung in NVS | **Optional** |

## Modul-Abhängigkeiten

```
IProfile ──→ IdryerRuntime ←── CloudStateMachine ←── IWifiManager
                                                  ←── ICredentialStore
                                                  ←── HttpApi ←── IHttpClient
                                                  ←── MqttClient

ActionDispatcher ──→ IdryerRuntime
```

`UartBridge` und `LinkIntegrationsManager` sind über den `CommandHandler` des Produkts (`setCommandHandler()`) verdrahtet und sind keine Kern-Abhängigkeiten.
