# MQTT-контракт

Файл `contracts/mqtt_contract.yaml` — точка правды для MQTT-интерфейса `idryer-core`.

## Область применения

Контракт описывает **только то, что реализует `MqttClient`** из `idryer-core`:

- топики, в которые библиотека умеет публиковать
- команды, которые библиотека принимает и маршрутизирует

Полный платформенный интерфейс (все команды от бэкенда для всех типов устройств, включая `drying`, `storage`, `profile`, `rfid`, и т.д.) находится в `contracts/portal_backend_status.md` — это [Platform Reference].

## Топики устройства (device → backend)

| Суффикс | Retained | Статус |
|---------|----------|--------|
| `info` | да | implemented |
| `telemetry` | нет | implemented |
| `status` | да | implemented |
| `config` | нет | implemented |
| `config/delta` | нет | implemented |
| `events` | нет | implemented |
| `integrations/status` | да | implemented |
| `offline` (LWT) | нет | implemented |

## Команды (backend → device)

| Суффикс | Обработчик | Статус |
|---------|-----------|--------|
| `commands/ping` | `IdryerRuntime` (built-in) | implemented |
| `commands/invoke` | `CommandHandler` продукта (рек.); fallback → `ActionDispatcher` | implemented |
| `commands/set` | `CommandHandler` продукта (рек.); fallback → `ActionDispatcher` | implemented |
| `commands/link_integration` | `LinkIntegrationsManager` via `CommandHandler` | implemented |
| `commands/bambu_apply` | `LinkIntegrationsManager` via `CommandHandler` | implemented |
| все остальные | `CommandHandler` продукта | product-defined |

## Правило изменения

Любое изменение MQTT-протокола в `idryer-core` должно одновременно затрагивать:

1. `contracts/mqtt_contract.yaml`
2. Код библиотеки (`mqtt_client.h/.cpp`)
3. Код портала / бэкенда

Сначала обновляется контракт, потом код.

## Совместимость

- Добавление новых необязательных полей в payload безопасно.
- Переименование существующих — требует одновременного обновления прошивки, портала и контракта.
- Payload `info` и `config` определяется продуктом — `idryer-core` их не validates.
