# iDryer Contracts

Единый источник правды для всех коммуникационных каналов платформы iDryer:
MQTT (portal/HA/Bambu), UART (RP2040↔ESP), WebSocket (Moonraker, local-WS),
HTTP REST (portal claim flow), HA discovery, WiFi provisioning.

## Если ты здесь впервые — TL;DR

```bash
# 1. Что в файле есть (карта):
python3 show.py

# 2. Найти что-то конкретное (например led.pulse Storage):
python3 show.py invoke_actions.storage_link.led.pulse

# 3. Список всех action'ов (всех продуктов) одним списком:
python3 show.py --actions

# 4. После правки yaml — обязательно:
./regen.sh        # validate + регенерация всех _generated/*
```

Не редактируй файлы в `_generated/` руками — они переписываются генераторами
и твои правки потеряются.

## Структура

```
mqtt_contract.yaml          ← source of truth (yaml)
mqtt_contract.schema.json   ← meta-schema (JSON Schema, валидирует yaml)

show.py                     ← навигатор: dotted-path выборка + JSON-примеры
validate_contract.py        ← валидация yaml + cross-refs + sizeof расчёт
gen_idryer_api_h.py         ← yaml → C++ facade enums/structs (iDryer_api.h)
gen_uart_protocol_h.py      ← yaml → C++ UART header
gen_mqtt_topics_h.py        ← yaml → C++ MQTT topics header
gen_ts_types.py             ← yaml → TypeScript types

regen.sh                    ← единая точка: validate + регенерация всего
pre_commit.sh               ← git hook: вызывает regen.sh + sync-check
HOOKS.md                    ← как установить hook

_generated/                 ← выходы генераторов (DO NOT EDIT)
  uart_protocol.h           ← C++ structs / enums / kind ids
  mqtt_topics.h             ← C++ topic constants / QoS / retained
  mqtt-api.types.ts         ← TS types для портала
```

## Навигация (`show.py`)

Контракт большой (~3000 строк yaml). Утилита `show.py` достаёт нужный кусок
по dotted-path, подсвечивает в терминале и снизу выводит готовые JSON-примеры
для `mosquitto_pub -m '...'`.

```bash
# Карта файла + список top-level секций
python3 show.py

# Только имена (без содержимого)
python3 show.py --list                     # top-level
python3 show.py --list invoke_actions      # дети секции

# Содержимое узла
python3 show.py invoke_actions                          # вся секция
python3 show.py invoke_actions.storage_link             # подсекция продукта
python3 show.py invoke_actions.storage_link.led.pulse   # один action
                                                        # (точка в имени работает)

# Плоский список всех invoke action'ов всех продуктов
python3 show.py --actions

# Конкретный enum / payload / message
python3 show.py enums.UartDeviceType
python3 show.py payloads.Telemetry
python3 show.py messages.command_drying
```

Опции:
- `--no-color` — отключить ANSI подсветку (или auto, если pipe в файл).
- `--no-examples` — не дописывать JSON-примеры в конец вывода.

Удобный alias (один раз в `~/.zshrc`):
```bash
alias contract='python3 ~/Projects/iDryerProject/docs/iDryer-Storage/lib/idryer-core/contracts/show.py'
# потом:  contract invoke_actions.storage_link.led.pulse
```

## Добавить новое устройство + виджет

Полный воркфлоу (fork → yaml → regen → firmware → widget → UIKit → PR):

→ **[docs/ru/09-add-product/02-add-widget.md](../docs/ru/09-add-product/02-add-widget.md)**
→ **[docs/en/09-add-product/02-add-widget.md](../docs/en/09-add-product/02-add-widget.md)**

Краткая схема:

```
mqtt_contract.yaml
  capability_vocabulary   ← новая периферия → hasXxx в Config
  canonical_roles         ← роль + имя React-виджета
  invoke_actions          ← args для команды виджета
  device_profiles         ← capabilities + invoke_actions устройства
        │
        └─► ./regen.sh
              ├─► _generated/scaffolds/my_device/  (firmware заготовка)
              ├─► mqtt-api.types.ts                 (TS типы)
              └─► portal/.../widgets/MyWidget.tsx   (копия виджета)
                        │
                        └─► widget-registry.tsx     (добавить вручную)
                        └─► UiKitPage.tsx           (добавить mock-секцию)
```

## Pipeline

```bash
./regen.sh
```

Внутри: `validate_contract.py` → все генераторы подряд. Список генераторов
держится в массивах `FIRMWARE_GENERATORS` / `ALL_GENERATORS` в `regen.sh`.

`pre_commit.sh` зовёт тот же `regen.sh` и проверяет что `_generated/*`
не разъехались с репо — см. `HOOKS.md`.

## Правило изменения

Любая правка коммуникационного канала обновляет **в одном changeset**:

1. `mqtt_contract.yaml`
2. регенерация `_generated/*`
3. код firmware / портала

Порядок строгий: **сначала контракт, потом код**. Pre-commit hook не даст
закоммитить yaml без актуальных `_generated/*`.

## Что покрыто в yaml

| Канал | Секция |
|---|---|
| MQTT (portal) | `messages`, `mqtt_only`, `legacy_command_topics` |
| UART (RP2040↔ESP) | `messages.bindings.uart`, `uart_only`, `uart_kind_ranges` |
| HA integration runtime | `ha_integration_topics` |
| HA Discovery | `ha_discovery_topics` |
| Bambu LAN MQTT | `bambu_lan_mqtt` |
| Moonraker WebSocket | `moonraker_websocket` |
| Local WebSocket server | `local_websocket` |
| Cloud HTTP API (claim) | `cloud_http_api` |
| WiFi provisioning | `wifi_provisioning` |

## Конвенции

- `rules.timestamp_convention` — поле `timestamp` (ISO 8601 UTC) в обе стороны.
- `rules.mqtt_session`, `rules.publish_qos`, `rules.uart_*` — общие транспортные правила.
- Расхождения и не-implemented entries фиксируются в `known_mismatches` с
  `decision_required` / `decision_made` / `decision_date`.

## Источники реальности (на момент составления)

- `idryer-core` (новая SDK): `lib/idryer-core/src/`
- `idryer-protocol` (legacy): `idryer-link/lib/idryer-protocol/src/`
- portal backend (NestJS): `iDryerPortal/backend/src/`
