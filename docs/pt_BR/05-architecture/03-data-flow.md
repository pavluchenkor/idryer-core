# Fluxo de dados

Descrição de como os dados se movem dentro de um dispositivo em execução. O objetivo é mostrar que \`idryer-core\` não usa um barramento de eventos nem um localizador de serviço: os participantes são conectados por ponteiros explícitos na raiz de composição, e cada direção de dados é um caminho separado e legível.

Padrões detalhados para "como rotear dados entre minhas partes" estão em [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md).

## Direções principais

\`\`\`
                Backend / app
                     │
                     │ MQTT commands/*
                     ▼
        ┌──────────────────────────────┐
        │  MqttClient                  │
        │  analisa topic + payload     │
        └──────────────┬───────────────┘
                       │
                       │ CommandCallback
                       ▼
        ┌──────────────────────────────┐
        │  IdryerRuntime               │
        │  ping → settimeofday + info  │
        │  outros → CommandHandler     │
        └──────────────┬───────────────┘
                       │
                       │ commandHandler_(cmd, data)
                       ▼
        ┌──────────────────────────────┐
        │  Produto handleCommand()     │
        │  invoke / set / get_config / │
        │  comandos específicos produto│
        └──────┬───────────────┬───────┘
               │               │
               ▼               ▼
   ActionDispatcher        IProfile
   handleInvoke / Set      getConfig
                           applyConfig
                           buildInfoJson
\`\`\`

\`\`\`
       Sensor (produto)        Perfil / executor
            │                           │
            │ tick() / leia             │ atualiza estado
            ▼                           ▼
       ┌───────────────────────────────────────┐
       │  Editor de produto                    │
       │  (StorageTelemetryPublisher, …)       │
       │  constrói JsonDocument                │
       └────────────────┬──────────────────────┘
                        │
                        │ pub.publishX(doc)
                        ▼
       ┌───────────────────────────────────────┐
       │  DevicePublisher (opcional)           │
       │  ajudante duplo publicar: MQTT + WS   │
       └─────────┬─────────────────────┬───────┘
                 │                     │
                 ▼                     ▼
            MqttClient            LocalAccess (WS)
            broker                cliente LAN
\`\`\`

## Comandos recebidos

1. **MQTT** entrega uma mensagem no tópico \`idryer/{serial}/commands/{cmd}\`.
2. \`MqttClient::handleMessage\` analisa a carga como JSON e chama \`CommandCallback\`.
3. \`CommandCallback\` é registrado por \`IdryerRuntime\` em \`begin()\` — aceita \`(command, data)\`, onde \`command\` é o sufixo após \`commands/\`.
4. \`IdryerRuntime::onMqttCommand\`:
   - Se \`command == "ping"\` — sincroniza tempo e publica informações. Não passado adiante.
   - Se um \`commandHandler_\` é registrado — passa tudo o mais para o produto.
   - Caso contrário — caminho integrado de fallback: \`invoke\` → \`ActionDispatcher\`, \`set\` → \`ActionDispatcher\`, \`device.getConfig\` → \`IProfile::getConfig\`.

5. **WS local** (se usado) aceita \`{"type":"command","command":"...","data":{...}}\`, desembrulha o envelope e chama o mesmo \`CommandSink\` registrado para o caminho MQTT. Um manipulador — dois transportes.

## Dados de saída

A biblioteca não publica nada a menos que seja solicitada. Todas as mensagens de saída são iniciadas pelo produto:

| O quê | Iniciado por | Via qual API |
|-------|-------------|----------|
| \`info\` | \`IdryerRuntime\` (uma vez quando Online e no \`ping\`) | \`MqttClient::publishInfoJson\` |
| \`telemetry\` | editor do produto | \`MqttClient::publishTelemetry\` ou \`DevicePublisher::publishTelemetry\` |
| \`status\` | código do produto na mudança de estado | \`MqttClient::publishStatus\` ou \`DevicePublisher::publishStatus\` |
| \`config\` | \`handleCommand\` em \`device.getConfig\` ou \`get_config\` | \`MqttClient::publishConfig\` |
| \`events\` | código do produto em um evento | \`MqttClient::publishEvent\` |
| \`integrations/status\` | \`LinkIntegrationsManager\` | \`MqttClient::publishIntegrationsStatus\` |
| \`offline\` | broker automaticamente (LWT) | dispositivo nunca publica isto |

## Conexões de objetos na raiz de composição

Referências entre participantes são passadas explicitamente através de construtores e setters. Sem registros globais.

\`\`\`
ArduinoWifiManager     ─┐
ArduinoCredentialStore ─┤
HttpApi (← Http)       ─┼──→ CloudStateMachine ──→ IdryerRuntime ──→ MqttClient
MqttClient             ─┘                              ▲
                                                       │
                                ActionDispatcher ──────┤
                                IProfile         ──────┘

                LocalAccess  ──── (setCommandSink) ────→ mesmo handleCommand
                DevicePublisher (&MqttClient, &LocalAccess)

                Sensor  ──→ Editor  ──→ DevicePublisher  ──→ MqttClient + LocalAccess
                Executor ←── ActionDispatcher (invoke)  ←── handleCommand
\`\`\`

Cada conexão é uma linha em \`main.cpp\`. Esta é a "raiz de composição explícita".

## Por quê este design

- **Sem magia**: para entender como os dados viajam de um sensor para a nuvem, o leitor vê a cadeia de ponteiros em \`main.cpp\`. Nenhum fluxo de dados fica escondido atrás de uma fachada.
- **Flexibilidade**: o produto escolhe se usar \`DevicePublisher\` (MQTT + WS), publicar apenas para MQTT ou usar seu próprio editor com lógica adicional.
- **Testabilidade**: cada nó é uma classe separada com dependências explícitas. Os nós podem ser substituídos por mocks sem alterar o resto da pilha.

## O que está intencionalmente ausente

- Nenhum barramento de eventos global ou intermediário de mensagens dentro do dispositivo.
- Nenhuma detecção automática de "tenho um sensor, publicarei seus dados por conta própria".
- Nenhum registro de tipo de "dispositivo conhece todos os seus provedores de telemetria".

Se o produto precisa de tais conexões — o produto as adiciona em seu próprio código. A biblioteca não as impõe.
