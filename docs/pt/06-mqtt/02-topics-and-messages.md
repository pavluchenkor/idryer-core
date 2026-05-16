# Tópicos e mensagens MQTT

Todos os tópicos têm a forma `idryer/{serial}/{suffix}`, onde `{serial}` é o número de série do dispositivo.

Este documento descreve os tópicos e comandos implementados pelo `MqttClient` do `idryer-core`. A interface completa da plataforma (todos os comandos de backend para todos os tipos de dispositivos) está em `contracts/portal_backend_status.md`.

## Dispositivo → backend

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

Publicado uma vez quando o dispositivo fica Online pela primeira vez e novamente ao receber um comando `ping`.

O payload é definido pelo produto via `IProfile::buildInfoJson()`. Campos esperados pelo backend no mínimo: `hardwareVersion`, `firmwareVersion`, `timestamp`.

Exemplo para Storage Link:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

### telemetry

```
idryer/{serial}/telemetry    retained=false    interval ~10 s
```

Publicado pelo produto via `pub.publishTelemetry()`. A biblioteca não publica automaticamente.

Exemplo para Storage Link (sensor de clima):

```json
{
  "units": [
    {"unitId": "U1", "temperature": 23.5, "humidity": 47.2}
  ]
}
```

### status

```
idryer/{serial}/status    retained=true    published on change
```

Publicado pelo produto na mudança de estado via `pub.publishStatus()`. O payload é definido pelo produto.

### config

```
idryer/{serial}/config    retained=false    on request
```

Publicado após receber `device.getConfig` (invoke) ou em resposta a `get_config`. Chamado via `pub.publishConfig()` ou `pub.publishConfigRaw()`.

Para payloads grandes (> 16000 bytes), publicado em blocos: cada bloco contém `tid`, `idx`, `total`, `last`, `d`.

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

Atualização parcial de configuração via `pub.publishConfigDelta()`. O backend espera um campo `d` (um objeto com as alterações).

### events

```
idryer/{serial}/events    retained=false    on event
```

Publicado pelo produto via `pub.publishEvent()`. A biblioteca não gera eventos automaticamente.

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

Publicado por `LinkIntegrationsManager`. Contém o estado da conexão de integração ativa.

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

Definido pelo broker automaticamente quando a conexão TCP cai. O dispositivo nunca publica este tópico manualmente.

## Backend → dispositivo

O dispositivo subscreve `idryer/{serial}/commands/#`.

### commands/ping

```
idryer/{serial}/commands/ping
```

Tratado directamente por `IdryerRuntime` — sincroniza a hora do sistema via `settimeofday()` e republica info.

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

Caminho preferido para acções do produto. A biblioteca passa o comando para o `CommandHandler` do produto (caminho recomendado). Se nenhum `CommandHandler` estiver registado, o comando cai para o `ActionDispatcher` incorporado (fallback).

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

A acção incorporada `device.getConfig` é tratada pelo runtime ou handler do produto — chama `IProfile::getConfig()` e publica o resultado.

### commands/set

```
idryer/{serial}/commands/set
```

Define um parâmetro de configuração único. Passado para o `CommandHandler` do produto (caminho recomendado). Fallback — `ActionDispatcher::handleSet()` incorporado se nenhum `CommandHandler` estiver registado.

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

Gestão de integrações. Tratada por `LinkIntegrationsManager` via `CommandHandler` do produto.

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```

### commands/bambu_apply

```
idryer/{serial}/commands/bambu_apply
```

Aplica parâmetros de filamento a um slot AMS na impressora Bambu. Tratado por `LinkIntegrationsManager`.

### Outros comandos de plataforma

Os comandos `drying`, `storage`, `profile`, `stop`, `get_config`, `read_rfid`, `write_rfid` e outros fazem parte da interface completa da plataforma iDryer. Não são tratados por `idryer-core` directamente; são entregues ao `CommandHandler` do produto. Referência: `contracts/portal_backend_status.md`.

## Formato de tópico

```c
// Construção de tópico
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

Constantes de sufixo são definidas em `mqtt/idryer_topics.h`:

```c
#define IDRYER_TOPIC_INFO               "info"
#define IDRYER_TOPIC_TELEMETRY          "telemetry"
#define IDRYER_TOPIC_STATUS             "status"
#define IDRYER_TOPIC_CONFIG             "config"
#define IDRYER_TOPIC_CONFIG_DELTA       "config/delta"
#define IDRYER_TOPIC_EVENTS             "events"
#define IDRYER_TOPIC_OFFLINE            "offline"
#define IDRYER_TOPIC_INTEGRATIONS_STATUS "integrations/status"
#define IDRYER_TOPIC_CMD_WILDCARD       "commands/#"
```
