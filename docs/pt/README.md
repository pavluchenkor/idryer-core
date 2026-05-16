# idryer-core — documentação da biblioteca

`idryer-core` — biblioteca C++ (Arduino/PlatformIO) para dispositivos iDryer baseados em ESP32. Gerencia WiFi, MQTT, a máquina de estados da nuvem e roteamento de comandos. O produto implementa apenas comportamento específico do dispositivo.

Esta é a documentação da **biblioteca**, não de nenhum produto específico.
A documentação do produto está localizada em [`docs/ru/`](../../docs/ru/).

---

## Início rápido

**Três coisas que você implementa:**

1. Implemente `IProfile` — cinco métodos (config, info, loop).
2. Monte `main.cpp` — objetos estáticos, passe dependências através de construtores.
3. Registre `handleCommand` — um manipulador único para MQTT e opcionalmente para WS local.

**Três coisas que a biblioteca faz:**

1. Gerencia WiFi → provisionamento → sessão MQTT.
2. Encaminha comandos recebidos para seu `handleCommand` (exceto `ping`, que é manipulado internamente).
3. Chama seus métodos `IProfile` nos momentos certos.

**O que você pode deixar intocado:**

- `ArduinoWifiManager`, `ArduinoCredentialStore` e outras classes `Arduino*` — use como estão, sem subclasses.
- `CloudStateMachine` — crie-a e passe-a para `IdryerRuntime`; ela se gerencia.
- `ActionDispatcher` — fallback de compatibilidade para invoke/set; para um novo produto, o tratamento de comandos passa por `setCommandHandler()`, não por `ActionDispatcher`.

Guia prático: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Exemplos funcionais: [`examples/`](../../examples/)

---

## Seções

| Seção | Descrição |
|-------|-----------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Propósito da biblioteca, o que não faz, quem usa |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Tabela de todos os módulos: propósito, opcionalidade |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Introdução rápida para um novo programador: o que conectar, programar e esperar |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | Como o produto monta a pilha: ordem de criação de objetos, padrão main.cpp |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | O que fica na biblioteca, o que fica no produto |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Fluxo de dados em um dispositivo em execução: comandos recebidos, mensagens enviadas, conexões |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | Classe `MqttClient`: construtor, conexão, publicação |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | Todos os tópicos MQTT: sequências, cargas úteis, retidas, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`: o que coordena, quais comandos manipula |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | Ponte UART para dispositivos com dois MCU |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker: configuração, limitações |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Implementações Arduino de interfaces de dispositivos |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | Interface `IProfile`, retornos de chamada, exemplo `LedStripProfile` |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`: propósito e regras de modificação |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Lista de verificação para construir um novo produto em cima de `idryer-core` |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | Problemas comuns: WiFi, provisionamento, MQTT, comandos, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | Como adicionar um sensor (fonte de dados) e publicar suas leituras |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | Como adicionar uma periférica e receber comandos |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | Como adicionar um transporte paralelo (BLE, HTTP, personalizado) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Receitas aplicadas para passar dados entre sensores / periféricos / perfil / editoras |
