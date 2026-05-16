# Contrato MQTT

O ficheiro `contracts/mqtt_contract.yaml` é a fonte da verdade para a interface MQTT do `idryer-core`.

## Âmbito

O contrato descreve **apenas o que `MqttClient` do `idryer-core` implementa**:

- tópicos que a biblioteca pode publicar
- comandos que a biblioteca aceita e encaminha

A interface completa da plataforma (todos os comandos de backend para todos os tipos de dispositivos, incluindo `drying`, `storage`, `profile`, `rfid`, etc.) está em `contracts/portal_backend_status.md` — esta é a [Referência da Plataforma].

## Tópicos de dispositivo (dispositivo → backend)

| Sufixo | Retido | Estado |
|--------|--------|--------|
| `info` | sim | implementado |
| `telemetry` | não | implementado |
| `status` | sim | implementado |
| `config` | não | implementado |
| `config/delta` | não | implementado |
| `events` | não | implementado |
| `integrations/status` | sim | implementado |
| `offline` (LWT) | não | implementado |

## Comandos (backend → dispositivo)

| Sufixo | Manipulador | Estado |
|--------|---------|--------|
| `commands/ping` | `IdryerRuntime` (incorporado) | implementado |
| `commands/invoke` | `CommandHandler` do produto (rec.); fallback → `ActionDispatcher` | implementado |
| `commands/set` | `CommandHandler` do produto (rec.); fallback → `ActionDispatcher` | implementado |
| `commands/link_integration` | `LinkIntegrationsManager` via `CommandHandler` | implementado |
| `commands/bambu_apply` | `LinkIntegrationsManager` via `CommandHandler` | implementado |
| todos os outros | `CommandHandler` do produto | definido pelo produto |

## Regra de mudança

Qualquer alteração no protocolo MQTT em `idryer-core` deve simultaneamente tocar:

1. `contracts/mqtt_contract.yaml`
2. Código da biblioteca (`mqtt_client.h/.cpp`)
3. Código de Portal / backend

Atualize o contrato primeiro, depois o código.

## Compatibilidade

- Adicionar novos campos opcionais a um payload é seguro.
- Renomear campos existentes requer atualizações simultâneas de firmware, portal e contrato.
- Os payloads `info` e `config` são definidos pelo produto — `idryer-core` não os valida.
