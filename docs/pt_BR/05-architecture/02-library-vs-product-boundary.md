# Limite: biblioteca e produto

## O que vive na biblioteca

A biblioteca (\`lib/idryer-core/\`) contém:

- A pilha de rede completa: WiFi, HTTP, MQTT, TLS.
- O protocolo de provisionamento/reivindicação.
- A máquina de estado da nuvem (\`CloudStateMachine\`).
- Ponte UART e protocolo de quadro.
- Clientes de integração (Bambu, HA, Moonraker).
- Interfaces de dispositivo (\`IWifiManager\`, \`ICredentialStore\`, \`IHttpClient\`, \`IProfile\`).
- Implementações Arduino dessas interfaces.
- Tópicos MQTT e lógica de publish/subscribe.

O teste para código pertencer à biblioteca: **qualquer produto com qualquer hardware pode usá-lo sem modificação**.

## O que vive no produto

O produto (\`src/\`) contém:

- Implementação \`IProfile\` — configuração, carga de informações, \`applyConfig\`.
- Lógica de negócio específica do dispositivo (controle LED, secagem, aquecimento).
- Manipuladores \`onInvoke\` / \`onSetCommand\`.
- Sensores de produto e publicação de telemetria.
- Inicialização periférica (FastLED, Wire, ImprovWiFi).
- Raiz de composição em \`main.cpp\`.

O teste para código pertencer ao produto: **sem alterar o hardware ou configuração, é sem sentido**.

## Exemplos concretos

| Código | Onde vive | Por quê |
|--------|-----------|--------|
| \`MqttClient\` | biblioteca | todo produto precisa de MQTT |
| \`CloudStateMachine\` | biblioteca | provisionamento/reivindicação é igual para todos |
| \`ArduinoWifiManager\` | biblioteca | conexão WiFi não depende do produto |
| \`LedStripProfile\` | produto | específico para Storage Link |
| \`LedStripExecutor\` | produto | controla FastLED, não necessário para outros dispositivos |
| \`Sht31ClimateSensor\` | produto | sensor específico para produto específico |
| \`StorageTelemetryPublisher\` | produto | conhece o formato de telemetria Storage Link |
| \`IProfile\` | biblioteca | contrato que a biblioteca chama |
| \`BambuClient\` | biblioteca | integração reutilizada em iDryer e iHeater |

## Interfaces como limite

A biblioteca conhece o produto apenas através de \`IProfile\`. Toda interação passa por cinco métodos:

\`\`\`cpp
profile->onOnline();               // biblioteca → produto: primeira vez online
profile->loop();                   // biblioteca → produto: cada ciclo
profile->buildInfoJson(buf, len);  // biblioteca → produto: carga de informações necessária
profile->getConfig(doc);           // biblioteca → produto: configuração necessária
profile->applyConfig(id, val);     // biblioteca → produto: comando de definição recebido
\`\`\`

O produto conhece a biblioteca através de \`MqttClient\` (para publicação de telemetria/eventos) e através de retornos de chamada \`ActionDispatcher\` (para comandos).

## O que não deve atravessar a limite

- A biblioteca não deve incluir cabeçalhos do produto.
- O produto não deve chamar \`CloudStateMachine::handleProvisioning()\` ou outros métodos privados da pilha diretamente — apenas através da API pública.
- Telemetria do produto é publicada diretamente via \`s_mqtt.publishTelemetry()\` — o tempo de execução não a vê.
