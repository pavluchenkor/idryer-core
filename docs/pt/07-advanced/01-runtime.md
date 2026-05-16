# IdryerRuntime

`IdryerRuntime` é o coordenador de dispositivo de nível superior. Conecta `CloudStateMachine`, `ActionDispatcher`, `IProfile` e `MqttClient` num único ponto de entrada: `begin()` / `loop()`.

## Construtor

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

Todos os quatro parâmetros são obrigatórios. `profile` pode ser `nullptr` (o runtime verifica antes de chamar seus métodos).

## Arranque

```cpp
void begin();
```

Realiza:

1. Regista um `CommandCallback` interno em `MqttClient`.
2. Chama `cloud->begin()`.

Chama uma vez em `setup()`, após `setCommandHandler()`.

## Loop principal

```cpp
void loop();
```

Em cada chamada:

1. Chama `cloud->loop()` — avança a máquina de estados.
2. Chama `profile->loop()` — lógica do produto.
3. Na primeira transição para Online:
   - Chama `profile->onOnline()`.
   - Chama `profile->buildInfoJson()` e publica o resultado em `idryer/{serial}/info` (retido).
4. Na perda de Online: redefine o sinalizador para que a próxima transição de Online funcione novamente.

## Manipulação integrada

### ping

```
commands/ping
```

Sempre manipulado pelo runtime — não passado para `CommandHandler`.

Extrai `data["timestamp"]` (formato `"YYYY-MM-DDTHH:MM:SSZ"`), sincroniza a hora do sistema via `settimeofday()`, depois republica o payload info.

## CommandHandler — o único ponto de extensão

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

Todos os comandos recebidos, excepto `ping`, são dirigidos para o `CommandHandler` registado.

Esta é a **única forma oficial** de estender a manipulação de comandos. Utilizado para que o MQTT e o transporte WS local convergam para um único ponto:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(data["action"] | "", "device.getConfig") == 0))
    {
        // Responder a ambos os transportes:
        s_pub.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // comandos específicos do produto...
}

// em setup():
runtime.setCommandHandler(handleCommand);   // MQTT
local.setCommandSink(handleCommand);        // WS local
```

!!! note "Se nenhum CommandHandler estiver registado"
    O runtime utiliza encaminhamento integrado: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `invoke device.getConfig` → publica config. Este é o comportamento predefinido — mantido para compatibilidade.

## Estado Online

```cpp
bool isOnline() const;
```

Retorna `true` se `CloudStateMachine` está no estado `Online`.

## O que o runtime não faz

- Não publica telemetria — essa é a responsabilidade do produto.
- Não gerencia directamente a reconexão MQTT — `CloudStateMachine` faz isso.
- Não conhece parâmetros de configuração específicos do dispositivo.
