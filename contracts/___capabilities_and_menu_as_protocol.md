# Capabilities и меню как декларативный протокол

## TL;DR — быстрая выжимка

**Что делаем:** Устройство само описывает свой UI через MQTT. Портал рендерит карточку автоматически — без хардкода под каждый тип устройства.

**Как работает:**
- Firmware: `menu.yaml` → генератор → C++ → при подключении публикует JSON-меню в MQTT
- JSON-меню содержит поле `r:` (canonical role) для пунктов которые должен знать портал
- `mqtt_contract.yaml` — единый словарь обоих концов: роль → тип виджета
- Portal: читает меню из кэша (DB) → находит пункты с known roles → рендерит виджеты

**Dashboard-карточка — два режима:**
- Известный deviceType → захардкоженный компонент (переходный режим, будет сокращаться)
- Нет хардкода → автосборка из кубиков по canonical roles

**Зачем:** Новое устройство = написать `menu.yaml` с ролями. Портал рисует карточку сам. Без изменений портала.

**Где что:**
- Словарь ролей: `mqtt_contract.yaml → canonical_roles`
- TypeScript для портала: `gen_ts_types.py` → `canonical-roles.ts` (автогенерация)
- Виджеты: `widget-registry.tsx` — новая роль без компонента = ошибка компиляции TypeScript
- Валидация прошивки: `pre_gen_menu.py` — роль не в контракте = ошибка сборки PlatformIO
- Что уже работает: см. §14

---

> **Статус: Этап 1 — в работе.** Архитектурные решения приняты (§10.2–10.6 закрыты).
> Часть шагов реализована — см. §14. Открытые вопросы отсутствуют.
>
> Документ описывает архитектуру обмена устройство ↔ портал, в которой
> портал строит UI декларативно из capabilities + меню без хардкодного
> знания типов устройств.
>
> **Этап 1 (в работе)**: iHeater Link + Storage Link — миграция
> на menu_protocol_v1. План — §13. Выполненное — §14.
>
> **Этап 2 (отдельная задача после стабилизации Этапа 1)**:
> сушилка (Dryer + iDryerRP2040) — переезд в новую модель. Сейчас
> в проде — не трогаем.
>
> Текущий контракт обмена — `mqtt_contract.yaml`. Этот документ — план
> его расширения и эволюции.

---

## 1. Предпосылки

### 1.1. Что было до исследования

Линейка iDryer состоит из нескольких типов устройств: `dryer`, `iheater_link`,
`storage_link` (новый). Каждый тип публикует MQTT-сообщения по контракту,
описанному в `mqtt_contract.yaml`. Портал (`iDryerPortal/backend`) их
обрабатывает.

Реальное состояние на момент исследования:

- **Портал не знал `storage_link` как тип устройства**. Функция
  `mapDeviceType` в `backend/src/mqtt-telemetry/device-type.util.ts:3-14`
  превращала любой неизвестный `deviceType` в `UNKNOWN`. Storage Link был
  «невидимкой» в системе. → **ИСПРАВЛЕНО**: `case 'storage_link'` добавлен,
  Prisma enum `STORAGE_LINK` добавлен.

- **Home Assistant integration** — реализована в idryer-core (`ha_builder.cpp`,
  `ha_publisher.cpp`). Устройства публикуют discovery в `homeassistant/` топики
  и автоматически видны в HA. Протестировано.

- **`info` каждого устройства имеет своё ad-hoc расширение**.
  Storage Link шлёт `unitsCount: 0` как неофициальный маркер «не сушилка»,
  iHeater пихает в `info` поля `capabilities` (uint32-битмаска),
  `scales[]`, `rfid[]`, `workTimeCounter`, `mcuSerial`. Контракт фиксирует
  эти расхождения как `product_variants.iheater_link.extra_fields`, но
  единого формата нет.

- **iHeater сериализует `capabilities` двумя способами одновременно**:
  как `uint32` битмаска и как nested-object под `units[0]`. Backend вынужден
  принимать оба формата (`known_mismatches.iheater_capabilities_dual_serialization`).

- **Команды от портала разрозненны**. Backend имеет 9-10 sender-методов:
  `sendDryingCommand`, `sendStorageCommand`, `sendProfileCommand`,
  `sendStopCommand`, `sendPauseCommand`, `sendResumeCommand`,
  `sendPingCommand` и др. Каждый шлёт в свой top-level топик
  (`commands/drying`, `commands/storage`, …). Прошивка имеет if-цепочку
  `command_routing.cpp:121-179` для распознавания каждой команды.

- **Универсальный `commands/invoke` существует в SDK, но почти не
  используется**. Контракт прямо отмечает: «Портал в продакшене этот
  action не использует — он шлёт `commands/get_config` напрямую. Действие
  фактически dead-code» (`mqtt_contract.yaml:1469`).

- **`info` Storage Link уже сейчас противоречит сам себе**: в `info`
  заявлено `unitsCount: 0`, но в `telemetry` Storage публикует `units[0]`
  с показаниями SHT31 (`mqtt_contract.yaml:756-758`). Контракт делает вид,
  что юнитов нет; код утверждает обратное.

### 1.2. Сформулированная проблема

Portal должен уметь **автоматически** строить UI на основе того, что
устройство о себе сообщает. Если завтра появится устройство нового типа
(или вариация существующего), portal должен:

1. Понять, какие физические компоненты есть (нагреватель, вентилятор,
   датчики, лента, весы, RFID).
2. Понять, какие действия устройство умеет выполнять (запуск сушки,
   запись метки, калибровка).
3. Понять параметры этих действий (диапазоны температуры, времени,
   текущие значения) — желательно per-unit, потому что юниты сушилки
   могут иметь разные настройки.
4. Сформировать UI без обновления самого портала.

Текущая модель «портал знает каждый deviceType в лицо» этого не даёт.

---

## 2. Цель исследования

Спроектировать модель обмена, в которой:

- **Портал не знает имён прошивки** (binding-имена, специфичные ID).
  Портал работает с **каноническими ролями** — стабильным словарём,
  зафиксированным в контракте.

- **Capabilities декларируют физическое устройство**, не действия.
  Действия и параметры живут в меню устройства.

- **Меню — единый источник истины для всего, что устройство умеет
  настраивать и делать**. И параметры (`type: value`), и действия
  (`type: action`) описаны в одном дереве.

- **Один путь обработки команд**: `commands/set` для изменения параметра,
  `commands/invoke` для запуска действия. Никаких отдельных топиков
  под каждый сценарий.

- **Пресеты на портале совместимы с любым устройством**, у которого есть
  нужные роли — без знания deviceType.

---

## 3. Анализ текущего состояния

### 3.1. Прошивка iHeater-link и iDryer

Меню (`menu_v2.yaml`) уже содержит и **параметры**, и **действия**:

```yaml
- id: dry_temp
  type: value
  bind: dry_temp
  min: 30
  max: 110
  scope: per_controller

- id: dry_time
  type: value
  bind: dry_time
  min: 0
  max: 600
  scope: per_controller

- id: dry_start
  type: action
  on_invoke: start_drying
```

Меню публикуется через `MenuBridge::publishFullConfig` →
`mqtt.publishConfigRaw` в топик `idryer/{serial}/config`. Формат
сжатый, со короткими ключами (`id, t, n, p, min, max, step, val`):

```json
{"v":8,"units":3,"active":0,"lang":"en",
 "menu":[{"id":3,"t":"val","n":["ТЕМПЕРАТУРА","TEMPERATURE"],"u":["°C","°C"],
          "p":2,"min":30,"max":110,"step":1,"val":[50,65,85]}]}
```

Per-unit значения публикуются как **позиционные массивы** `val: [50, 65, 85]`,
где индекс соответствует юниту. Это уже работающий per-unit формат.

`scope: per_controller` означает «значение хранится отдельно для каждого
юнита». `scope: global` — общая настройка устройства.

Действия (`type: action`) имеют хук `on_invoke: <function_name>`, который
дёргается при выборе action в локальном меню (физическое нажатие
энкодера).

### 3.2. Существующий `ActionDispatcher` в SDK

`lib/idryer-core/src/cloud/action_dispatcher.h` уже реализует механизм
`commands/invoke`:

```cpp
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);
void setInvokeHandler(InvokeHandler fn, void* ctx);
```

То есть архитектурно SDK готов к canonical-action модели. Используется он
сейчас минимально — практически только для `led.pulse` Storage Link.

### 3.3. Backend portal

Команды отправляются 9-10 разными методами в разные топики. Параметры
для каждой команды формируются индивидуально. Парсинг ответов привязан
к типам устройств. Пресеты хранятся как структуры с полями
`{temperature, duration, humidity, ...}` — без ссылки на канонические
имена.

---

## 4. Эволюция решения

В ходе исследования было рассмотрено несколько подходов; ниже — путь,
который привёл к финальной модели.

### 4.1. Шаг 1. Capabilities как набор bool

Первая гипотеза: capabilities — это плоский набор bool-флагов
(`heater: true`, `tempAir: true`), массивы для слотов (`scales: [0,1]`),
enum для ленты. Достаточно для feature detection.

**Контр-аргумент**: portal должен знать диапазоны параметров, чтобы
рисовать слайдеры. `heater: true` ничего не говорит о `maxTemp`.

### 4.2. Шаг 2. Capabilities как объект с параметрами

Вторая гипотеза: каждый capability — объект с лимитами:
`heater: { tempRange: [0, 80], durationRange: [0, 1440] }`.

**Контр-аргумент 1**: диапазоны зависят от режима. У одной сушилки
`drying.tempRange = [30, 110]`, `storage.tempRange = [35, 90]`. Простой
`heater.tempRange` теряет эту информацию.

**Контр-аргумент 2**: диапазоны и текущие значения **уже публикуются в
config** (как поля `min`, `max`, `step`, `val` в menu items). Дублирование
этой информации в capabilities — раздувание.

### 4.3. Шаг 3. Canonical roles в меню

Третья гипотеза: добавить поле `role` в menu_meta. Каждый пункт меню,
который должен быть распознан порталом, получает каноническое имя:

```yaml
- id: dry_temp
  bind: dry_temp
  role: drying.target_temperature
```

В payload config выезжает короткое поле `r`:

```json
{"id":2,"bind":"dry_temp","r":"drying.target_temperature",
 "min":30,"max":110,"val":[60,65,70,75]}
```

Портал по `role` находит menu item, берёт `min/max/val` и рисует слайдер.
Прошивка свободна в `bind`-именах, `role` — формальный контракт между
устройством и порталом.

### 4.4. Шаг 4. Меню как словарь действий

Финальный шаг: понять, что **меню уже содержит actions** (`dry_start`
с `on_invoke: start_drying`). Если расширить роли и на actions, и
расширить `commands/invoke` для вызова menu actions по id, то:

- никаких отдельных canonical_actions не нужно;
- никаких top-level топиков команд (`commands/drying`, `commands/storage`)
  не нужно;
- все команды унифицированы как `commands/set` (изменить параметр)
  и `commands/invoke` (вызвать action);
- словарь canonical_roles **закрывает и параметры, и действия**.

Меню становится **декларативным описанием возможностей устройства**.
Портал — движок UI на этом описании.

---

## 5. Финальная модель

### 5.1. Принципы

1. **info** описывает физическое устройство: какие физические компоненты
   на борту, сколько юнитов, какие интеграции поддерживает прошивка.
2. **config** описывает что устройство умеет настраивать и делать:
   полное меню с параметрами и actions.
3. **commands/set** изменяет параметр меню. **commands/invoke** вызывает
   action меню. Этого достаточно для **всех** управляющих команд.
4. **canonical_roles** — закрытый словарь в контракте. Только эти имена
   портал знает наизусть. Всё остальное портал получает из config
   конкретного устройства.
5. **Адресация юнита через активный юнит** (`controller_choice` в меню).
   Портал переключает активный юнит отдельной командой `set`, далее
   все `set/invoke` применяются к нему. Никакого `unitId` в payload
   `set/invoke`.
6. **Capabilities = только физика**. Никаких `modes`, никаких диапазонов
   в capabilities. Список запускаемых режимов выводится из меню (наличие
   actions с известными ролями).

### 5.2. Что меняется относительно текущего

**Топики, которые исчезают** (переезжают в `commands/invoke` по id action):

- `commands/drying`
- `commands/storage`
- `commands/profile`
- `commands/stop`
- `commands/pause`
- `commands/resume`
- `commands/find`
- `commands/clear_errors`
- `commands/read_rfid`
- `commands/write_rfid`
- `commands/link_integration`
- `commands/bambu_apply`

**Топики, которые остаются**:

- `commands/set` — изменение параметра меню
- `commands/invoke` — вызов action меню
- `commands/get_config` — запрос полного меню (system)
- `commands/ping` — time-sync (system)

**Поля, которые исчезают из info**:

- `unitsCount` (заменяется `units.length`)
- iHeater extra_fields (`capabilities` как uint32, дублированные `scales/rfid`)

**Поля, которые добавляются в info**:

- `unitCapabilities` — общий блок физических capabilities всех юнитов
- `leds` — описание ленты на корне (или внутри юнита, если лента per-unit)
- `integrations` — массив поддерживаемых интеграций

**Поля, которые добавляются в config menu items**:

- `r` — каноническая роль пункта меню (для `value`, `toggle`, `action`)

---

## 6. Полные форматы обмена

### 6.1. Топология MQTT

Все топики имеют префикс `idryer/{serial}/`.

```
УСТРОЙСТВО → ПОРТАЛ
  info                    retained, при онлайне и ping
  telemetry               периодически (~5 сек)
  status                  retained, при изменении
  config                  по запросу + при изменении
  config/delta            опционально, частичные обновления
  events                  при событиях (ack, error, alarm)
  weights                 при изменении веса
  rfid                    rfid-события
  integrations/status     retained, состояние HA/Bambu/Moonraker
  offline                 LWT, broker-side

ПОРТАЛ → УСТРОЙСТВО
  commands/invoke         вызов action из меню
  commands/set            изменение параметра меню
  commands/get_config     запрос полного меню
  commands/ping           time-sync (system)
```

### 6.2. info — устройство представляется

#### Общая структура

```json
{
  "deviceType": "<строка типа>",
  "firmwareVersion": "x.y.z",
  "hardwareVersion": "x.y",
  "mcuSerial": "...",                    // опционально, для двух-MCU устройств
  "leds": { "count": <N> },              // опционально, общая лента устройства
  "integrations": ["ha", "bambu", ...],  // массив поддерживаемых интеграций
  "unitCapabilities": {
    "heater": true|false,
    "fan": true|false,
    "servo": true|false,
    "tempAir": true|false,
    "tempHeater": true|false,
    "rhAir": true|false
  },
  "units": [
    {
      "unitId": "U1",
      "scales": [<глобальные индексы>],
      "rfid":   [<глобальные индексы>]
    },
    ...
  ],
  "timestamp": "ISO 8601"
}
```

`unitCapabilities` — **общий** для всех юнитов одного устройства.
Различия между юнитами выражены только через `scales` / `rfid` и через
config (per-unit values).

#### Пример: Storage Link

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "leds": { "count": 30 },
  "integrations": [],
  "unitCapabilities": {
    "tempAir": true,
    "rhAir": true
  },
  "units": [
    { "unitId": "U1", "scales": [], "rfid": [] }
  ],
  "timestamp": "2026-05-05T10:00:00Z"
}
```

#### Пример: iHeater Link

```json
{
  "deviceType": "iheater_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "mcuSerial": "AABBCCDDEEFF1122",
  "integrations": ["ha", "bambu", "moonraker"],
  "unitCapabilities": {
    "heater": true,
    "fan": true,
    "tempAir": true
  },
  "units": [
    { "unitId": "U1", "scales": [], "rfid": [] }
  ],
  "timestamp": "2026-05-05T10:00:00Z"
}
```

#### Пример: iDryer (4 юнита, общая лента)

```json
{
  "deviceType": "dryer",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "mcuSerial": "AABBCCDDEEFF1122",
  "leds": { "count": 60 },
  "integrations": [],
  "unitCapabilities": {
    "heater": true,
    "fan": true,
    "servo": true,
    "tempAir": true,
    "tempHeater": true,
    "rhAir": true
  },
  "units": [
    { "unitId": "U1", "scales": [0, 1], "rfid": [0, 1] },
    { "unitId": "U2", "scales": [2, 3], "rfid": [2, 3] },
    { "unitId": "U3", "scales": [4, 5], "rfid": [4, 5] },
    { "unitId": "U4", "scales": [6, 7], "rfid": [6, 7] }
  ],
  "timestamp": "2026-05-05T10:00:00Z"
}
```

### 6.3. config — что устройство умеет

Текущий формат `MenuBridge::publishFullConfig` сохраняется. Добавляется
поле `r` (role) в menu items, которые должны быть распознаны порталом.

```json
{
  "v": 8,
  "active": 0,
  "lang": "ru",
  "menu": [
    { "id": 0, "t": "sub", "n": ["МЕНЮ", "MENU"], "p": -1 },

    { "id": 1, "t": "sub", "n": ["СУШКА", "DRYING"], "p": 0 },

    { "id": 2, "t": "val", "p": 1, "bind": "dry_temp",
      "r": "drying.target_temperature",
      "n": ["ТЕМПЕРАТУРА", "TEMPERATURE"], "u": ["°C", "°C"],
      "min": 30, "max": 110, "step": 1, "val": [60, 65, 70, 75] },

    { "id": 3, "t": "val", "p": 1, "bind": "dry_time",
      "r": "drying.duration",
      "n": ["ВРЕМЯ", "TIME"], "u": ["мин", "min"],
      "min": 0, "max": 600, "step": 1, "val": [240, 240, 240, 240] },

    { "id": 4, "t": "act", "p": 1, "bind": "dry_start",
      "r": "drying.start",
      "n": ["СТАРТ", "START"] },

    { "id": 5, "t": "sub", "n": ["ХРАНЕНИЕ", "STORAGE"], "p": 0 },

    { "id": 6, "t": "val", "p": 5, "bind": "storage_temp",
      "r": "storage.target_temperature",
      "n": ["ТЕМПЕРАТУРА", "TEMPERATURE"], "u": ["°C", "°C"],
      "min": 35, "max": 90, "step": 1, "val": [45, 45, 45, 45] },

    { "id": 7, "t": "val", "p": 5, "bind": "storage_hum",
      "r": "storage.target_humidity",
      "n": ["ВЛАЖНОСТЬ", "HUMIDITY"], "u": ["%RH", "%RH"],
      "min": 5, "max": 30, "step": 1, "val": [12, 12, 12, 12] },

    { "id": 8, "t": "act", "p": 5, "bind": "storage_start",
      "r": "storage.start",
      "n": ["СТАРТ", "START"] },

    { "id": 9, "t": "act", "bind": "stop",
      "r": "common.stop",
      "n": ["СТОП", "STOP"] },

    { "id": 10, "t": "val", "bind": "controller_choice",
      "r": "system.active_unit",
      "n": ["АКТИВНЫЙ ЮНИТ", "ACTIVE UNIT"],
      "min": 0, "max": 3, "step": 1, "val": 0 }
  ]
}
```

Пункты без `r` — приватные для устройства; портал может показать как
сырое поле в админке, но не использует в управлении.

Виджет для пункта меню определяется **исключительно** через `canonical_roles` в контракте
по полю `r:`. Устройство не шлёт имя виджета — портал смотрит его сам:

```
r: "led.pulse" → CanonicalRoles["led.pulse"].widget = "LedPulse" → LedPulseWidget
r: "common.stop" → CanonicalRoles["common.stop"].widget = "button" → ButtonWidget
```

Реестр виджетов хранится на портале. Расширение каталога = добавить роль в контракт +
реализовать компонент на портале.

### 6.4. telemetry, status, weights, rfid, events

Не меняются — остаются в текущем формате.

### 6.5. Команды портал → устройство

#### commands/set — изменить параметр

По `id` (предпочтительно) или по `bind` (для отладки):

```json
{ "id": 2, "val": 70, "timestamp": "2026-05-05T10:00:00Z" }
```

```json
{ "bind": "dry_temp", "val": 70, "timestamp": "2026-05-05T10:00:00Z" }
```

Применяется к **активному** юниту (для `scope: per_controller`) или
глобально (для `scope: global`).

#### commands/invoke — вызвать action

Без параметров:

```json
{ "id": 4, "timestamp": "2026-05-05T10:00:00Z" }
```

С runtime-параметрами (для actions, которые принимают ad-hoc args —
RFID write, LED pulse и т.п.):

```json
{ "id": 25, "args": { "tagData": "AABB..." }, "timestamp": "2026-05-05T10:00:00Z" }
```

#### commands/get_config — запросить меню

```json
{ "timestamp": "2026-05-05T10:00:00Z" }
```

Устройство в ответ публикует `config`.

#### commands/ping — синхронизация времени

```json
{ "timestamp": "2026-05-05T10:00:00Z" }
```

---

## 7. End-to-end сценарии

### 7.1. Запустить сушку на U2 с T:70°C, t:120 мин

```
Пред-условие: портал имеет config устройства.
Портал ищет в config:
  role "system.active_unit"          → id 10
  role "drying.target_temperature"   → id 2
  role "drying.duration"             → id 3
  role "drying.start"                → id 4

1. PUB commands/set    { "id": 10, "val": 1 }    // активный = U2
2. PUB commands/set    { "id": 2,  "val": 70 }   // температура
3. PUB commands/set    { "id": 3,  "val": 120 }  // время
4. PUB commands/invoke { "id": 4 }               // запуск drying.start

Прошивка вызывает on_invoke = start_drying для активного (U2).

5. Устройство публикует обновлённый status.
6. (Опционально) устройство публикует events { type: "ack", action: "drying.start" }.
7. Telemetry продолжается каждые ~5 сек.
```

### 7.2. Применить пресет «PETG normal» к U3

```
Пресет на портале (хранится в терминах canonical_roles):
  { "name": "PETG normal",
    "params": [
      { "role": "drying.target_temperature", "val": 60 },
      { "role": "drying.duration",           "val": 180 }
    ],
    "trigger": { "role": "drying.start" } }

1. PUB commands/set    { "id": 10, "val": 2 }    // активный = U3
2. PUB commands/set    { "id": 2,  "val": 60 }   // role drying.target_temperature → id 2
3. PUB commands/set    { "id": 3,  "val": 180 }  // role drying.duration → id 3
4. PUB commands/invoke { "id": 4 }               // role drying.start → id 4
```

Пресет работает с **любым** устройством, у которого есть эти роли.
Не нужно знать deviceType, не нужно знать имена bind конкретной прошивки.

### 7.3. Считать RFID на ридере 1

```
В config есть action:
  { "id": 25, "t": "act", "bind": "rfid_read", "r": "rfid.read", ... }

1. PUB commands/invoke { "id": 25, "args": { "readerId": 1 } }

2. Прошивка читает метку и публикует:
   PUB idryer/{serial}/rfid
       { "event": "tag_detected", "readerId": 1, "tag": "AABB...", "unitId": "U1" }
```

---

## 8. Словарь canonical_roles

Закрытый словарь в `mqtt_contract.yaml`. Единственный hardcoded элемент,
который portal знает наизусть.

Для каждой роли указано:
- `type` — тип данных
- `unit` — единица измерения (опционально)
- `widget` — какой UI-компонент рисовать на портале (`slider`/`number`/
  `toggle`/`select`/`button` — стандартные; имена типа `ProfileEditor`/
  `RfidWriter`/`LedPulse` — хардкод-компоненты)
- `args_schema` — для actions с runtime-параметрами (опционально)

```yaml
canonical_roles:
  # ── Сушилка (Dryer) ────────────────────────────────────────────────
  drying.target_temperature:    { type: float, unit: "°C",  widget: slider }
  drying.duration:              { type: uint,  unit: "min", widget: slider }
  drying.start:                 { type: action, widget: button }
  drying.stop:                  { type: action, widget: button }

  storage.target_temperature:   { type: uint,  unit: "°C",  widget: slider }
  storage.target_humidity:      { type: uint,  unit: "%RH", widget: slider }
  storage.start:                { type: action, widget: button }
  storage.stop:                 { type: action, widget: button }

  profile.start:                { type: action, widget: ProfileEditor,
                                  args_schema: { stages: array } }
  profile.stop:                 { type: action, widget: button }

  rfid.read:                    { type: action, widget: button,
                                  args_schema: { readerId: int } }
  rfid.write:                   { type: action, widget: RfidWriter,
                                  args_schema: { tagData: base64 } }

  # ── iHeater Link ───────────────────────────────────────────────────
  iheater.material_petg:        { type: uint, unit: "°C", widget: slider }
  iheater.material_pla:         { type: uint, unit: "°C", widget: slider }
  iheater.material_pacf:        { type: uint, unit: "°C", widget: slider }
  iheater.bambu_enabled:        { type: bool, widget: toggle }
  iheater.moonraker_enabled:    { type: bool, widget: toggle }
  iheater.ha_enabled:           { type: bool, widget: toggle }

  # ── Storage Link (полка-индикатор) ─────────────────────────────────
  storage.led_count:            { type: uint, widget: number }
  storage.led_brightness:       { type: uint, widget: slider }
  storage.led_animation:        { type: uint, widget: select }
  led.pulse:                    { type: action, widget: LedPulse,
                                  args_schema: { ledIndex: int, color: rgb,
                                                 durationMs: uint } }

  # ── Общие действия ─────────────────────────────────────────────────
  common.stop:                  { type: action, widget: button }
  common.find:                  { type: action, widget: button }
  common.clear_errors:          { type: action, widget: button }

  # ── Системное ──────────────────────────────────────────────────────
  system.active_unit:           { type: uint, widget: hidden }
  system.language:              { type: uint, widget: select }
```

### Виджет определяется через canonical_roles

Портал смотрит `CanonicalRoles[item.r].widget`. Каждая роль в контракте
**обязана** иметь `widget:`. Устройство поле `widget` не публикует.

---

## 9. План реализации

### 9.1. Прошивка (firmware)

1. **menu_v2.yaml**: добавить поле `role:` в нужные пункты меню
   (как для `value`, `toggle`, так и для `action`).
2. **`gen_menu_v2.py`**: расширить генератор — парсить `role` из YAML,
   класть в `menu_meta.h`.
3. **`menu_meta.h`**: добавить поле `const char* role` в структуру
   `MenuMeta`.
4. **`menu_buildFullJson` (`menu_commands.h:182`)**: при сериализации
   menu item выводить поле `r` если задан. Поле `widget` устройство
   не публикует — портал определяет виджет из `canonical_roles` по `r`.
5. **`pre_gen_menu.py` — валидация ролей**: при генерации проверять каждый
   `role:` из `menu.yaml` против `canonical_roles` в `mqtt_contract.yaml`.
   Если роль не найдена — сборка падает с ошибкой. Разработчик не может
   использовать неизвестную роль молча.
6. **`MenuBridge::applySetCommand`**: уже работает в текущем виде,
   изменений не требует. Адресация юнита — через активный
   контроллер (`controller_choice`), как сейчас.
6. **`MenuBridge::applyInvokeCommand`**: добавить — вызов `on_invoke`
   action по `id` (или по `bind` для отладки), с поддержкой
   опциональных `args` для action-ов с runtime-параметрами.
7. **`command_routing.cpp`**: упростить до 4 команд:
   - `set` → `MenuBridge::applySetCommand`
   - `invoke` → `MenuBridge::applyInvokeCommand`
   - `get_config` → `MenuBridge::publishFullConfig`
   - `ping` → time-sync (как сейчас)

   Старые `drying`, `storage`, `profile`, `stop`, `pause`, `resume`,
   `find`, `clear_errors`, `read_rfid`, `write_rfid`,
   `link_integration`, `bambu_apply` — удалить (legacy переезд).

### 9.2. Backend (portal)

1. **`mqtt-api.types.ts`**: обновить `InfoPayload`:
   - убрать `unitsCount` (использовать `units.length`);
   - убрать `UnitCapabilities` per-unit, ввести `unitCapabilities` на корне;
   - добавить `leds` на корне;
   - привести имена к camelCase (`tempAir`, `rhAir`, `tempHeater`).

2. **`info.handler.ts`**:
   - распарсить новые поля;
   - удалить лог-текст `'Monitor' / 'Single multi-filament' / 'Multi-chamber'`;
   - сохранить `unitCapabilities` и `leds` в `Device.hardwareConfig`.

3. **`device-type.util.ts:3-14`**: добавить `case 'storage_link': return DeviceType.STORAGE_LINK;`.

4. **Prisma**: миграция `enum DeviceType` — добавить `STORAGE_LINK`.

5. **`mqtt-telemetry.service.ts`**: заменить 9-10 sender-методов на два:
   - `sendSet(deviceToken, id, val)` → publish `commands/set`
   - `sendInvoke(deviceToken, id, args?)` → publish `commands/invoke`

   Старые методы (`sendDryingCommand` и др.) — переписать как обёртки
   над `sendInvoke` с предварительным маппингом role → id из последнего
   полученного config устройства.

6. **`devices.service.ts`**: добавить хелпер
   `getMenuIdByRole(deviceId, role)` для маппинга canonical role → menu id.

7. **`presets`**: переписать на роли. Поле `temperature` → `role: "drying.target_temperature"`, и т.д.

### 9.3. Контракт

1. **`mqtt_contract.yaml`**: добавить раздел `canonical_roles` —
   закрытый словарь.
2. Обновить `messages.info`: новый формат с `unitCapabilities` на корне.
3. Обновить `messages.config_full`: добавить документацию поля `r`.
4. Удалить `messages.command_drying`, `command_storage`, `command_profile`,
   `command_stop`, `command_find`, `command_read_rfid`, `command_clear_errors`
   и пр. — все эти команды переезжают в `commands/invoke`.
5. Удалить `product_variants.iheater_link.extra_fields` (закрывается
   единым форматом).
6. Удалить или перефразировать исключение `unit_id_format` — `unitId`
   везде в формате `U1..U4` (numeric только в UART, который остаётся
   как есть).

---

## 10. Открытые вопросы

### 10.1. Атомарность пуска

Запуск сейчас атомарен (один MQTT message `commands/drying`). Новая
модель — три-четыре message (`set` × N + `invoke`). Между ними возможны:

- параллельная команда от другого юзера портала (race);
- сетевая потеря одного из set'ов (QoS 1 защищает, но задержка возможна).

**Решение**: QoS 1 + persistent session. На portal-стороне — лок
«один юзер за раз делает действия с устройством» (уже частично есть).

### 10.2. Профиль с N stages — РЕШЕНО

Profile с 5 этапами по 3 параметра не вписывается в плоское меню. Принят
**вариант (b): `invoke` с `args` + захардкоженный виджет `ProfileEditor`**.

В меню — один пункт `act` с `r: profile.start`. Портал смотрит
`CanonicalRoles["profile.start"].widget = "ProfileEditor"` и рисует захардкоженный
React-компонент с таблицей этапов (add/remove строк, поля для каждой
стадии). При нажатии «Применить» портал шлёт:

```json
PUB commands/invoke
{ "id": <profile_start_id>,
  "args": { "stages": [
    { "T": 50, "ramp": 600, "hold": 1800 },
    { "T": 80, "ramp": 600, "hold": 7200 }
  ]}
}
```

Прошивка обрабатывает `args` в `applyInvokeCommand` и стартует профиль.

**Отклонены**:
- Bulk-set — лишняя сложность на стороне прошивки.
- Профиль как N menu stages — структурно невозможно при переменной длине.

### 10.3. Indexed-роли — ОТМЕНЕНО

Раз профиль = роль `profile.start` → виджет `ProfileEditor` из `canonical_roles`, роли на каждый
этап (`profile.stage.temperature` с index'ом) **не нужны**. Хардкод-виджет
сам управляет своей структурой данных, отдельные роли для UI не
требуются.

### 10.4. unitId в payload — ОТМЕНЕНО

`unitId` в `commands/set` / `commands/invoke` **не вводим**. Адресация
через активный юнит (`system.active_unit`) достаточна. Портал
переключает активный юнит одним `set`, дальше все команды идут на него.

Если в будущем (после работы в проде) станет узким местом для частых
multi-юнит сценариев — пересмотрим. На старте — нет.

### 10.5. Версионирование канонических ролей

Контракт версионируется атомарно — изменение в `canonical_roles` означает
одновременную миграцию прошивки и портала. Промежуточные форматы и
deprecated-aliases не вводятся.

При добавлении новой роли — старые устройства её не публикуют, портал
просто не видит соответствующий виджет. Деградация естественная.

### 10.6. Порядок миграции продуктов — РЕШЕНО

Миграция в два этапа:

**Этап 1 — сейчас. Переезжают:**
- **iHeater Link** — добавление `r:` в его меню (mat_petg/mat_pla/
  mat_pacf/bambu_en/moon_en/ha_en); удаление топиков
  `commands/link_integration`, `commands/bambu_apply` (переезжают в
  `commands/set` соответствующих menu items).
- **Storage Link** — добавление `r:` в его меню (LED count, brightness,
  animation); починка `mapDeviceType` в backend (сейчас Storage Link
  падает в `UNKNOWN`); добавление виджета `LedPulse` для роли
  `led.pulse`.

**Этап 2 — отдельной задачей, после стабилизации Этапа 1:**
- **Сушилка (iDryer Dryer + iDryerRP2040)** — добавление `r:` во все
  релевантные menu items, переезд `commands/drying`/`storage`/`profile`/
  `read_rfid`/`write_rfid` в `set`+`invoke`. Backend старые handlers
  (`weights.handler.ts`, `rfid.handler.ts`) переписать на чтение через
  единый telemetry/events-канал.

Сушилка сейчас работает в проде — её **не трогаем** до отдельного
согласования. Без shim-слоёв в backend (нет полевого парка устройств с
произвольными прошивками — owner сам пишет все три).

**Зафиксировать в `mqtt_contract.yaml`:**

```yaml
migration_status:
  iheater_link:  { target: menu_protocol_v1, status: in_progress }
  storage_link:  { target: menu_protocol_v1, status: in_progress }
  dryer:         { target: menu_protocol_v1, status: deferred,
                   note: "Сушилка работает в проде. Миграция отдельной
                          задачей после стабилизации iHeater и Storage." }
```

---

## 11. Преимущества модели

1. **Portal не знает имён прошивки**. Работает с canonical_roles.
2. **Меню = единый источник истины**. И параметры, и действия в одном
   дереве с одной системой адресации.
3. **Capabilities компактны и честны** — описывают только физику.
4. **Унификация команд**: четыре топика вместо 14.
5. **Прошивка имеет один диспатчер** (`MenuBridge`) для всех команд.
6. **Backend имеет два sender-метода** вместо 10.
7. **Пресеты независимы от deviceType** — работают с любым устройством,
   у которого есть нужные роли.
8. **Самодокументация контракта** — `canonical_roles` + `info` + `config`
   полностью описывают возможности устройства.
9. **Расширяемость**: новый продукт = новые menu items + role'ы,
   без изменения portal.
10. **Закрытие текущих расхождений**:
    - двойная сериализация iHeater capabilities;
    - Storage Link `unitsCount: 0` vs telemetry с `units[0]`;
    - PascalCase vs camelCase в UnitCapabilities.

---

## 12. Риски и ограничения

1. **Большой объём миграции**. Backend, прошивка ×3 продукта,
   тесты, документация. Месяц-полтора активной работы.
2. **Нет постепенной миграции с гарантией compat**. Top-level топики
   удаляются, не заменяются deprecated-aliases. Прошивки и backend
   выкатываются согласованно.
3. **Контракт становится центральным документом** — без него ни
   прошивку, ни backend сделать нельзя. Дисциплина обновления контракта
   критична.
4. **Сложные команды (профиль, RFID write) требуют дополнительных
   решений** — bulk set или action с args. См. §10.2, 10.3.
5. **Атомарность пуска уходит**. Multi-message запуск требует
   аккуратной координации. См. §10.1.

---

## 13. Что делать дальше

Открытые вопросы §10.2-10.4 закрыты решениями. План конкретный.

### Шаг 1 — `mqtt_contract.yaml`
1. ~~Добавить раздел `canonical_roles` (как в §8).~~ **✅ DONE** — раздел присутствует.
2. Добавить раздел `migration_status` (как в §10.6).
3. Удалить из `commands` секции: `commands/drying`, `storage`, `profile`,
   `stop`, `pause`, `resume`, `find`, `clear_errors`, `read_rfid`,
   `write_rfid`, `link_integration`, `bambu_apply` — для устройств в
   статусе `target: menu_protocol_v1`. Для `dryer` (status: deferred)
   оставить.
4. Документировать в `messages.config_full` поле `r:`.

### Шаг 2 — SDK (`idryer-core`)
1. `MenuBridge::applyInvokeCommand` — добавить (вызов action по id, с
   опциональным `args`).
2. `command_routing` — упростить до 4 веток: `set`/`invoke`/`get_config`/
   `ping`. Удалить ветки `drying`/`storage`/etc. для iHeater и Storage.
3. `gen_menu_v2.py` — парсить `role:` из YAML, выводить в
   `menu_meta.h` (поле `const char* role`).
4. `menu_buildFullJson` — выводить `r:` в config-payload когда задан.

### Шаг 3 — iHeater Link и Storage Link
1. `iHeater-link/src/menu/menu.yaml`: добавить `r:` для mat_*, *_en
   полей. Удалить ручные обработки `link_integration`/`bambu_apply` из
   `main.cpp` (переезжают в `set`).
2. `iDryer-Storage/src/menu/menu.yaml`: добавить `r:` для led_count,
   brightness, animation. Добавить пункт `act` с `r: led.pulse`.
3. Прошивка обоих устройств — собрать, залить, протестировать `set`
   и `invoke` через mosquitto.

### Шаг 4 — Backend portal
1. ~~`device-type.util.ts:3-14` — добавить `case 'storage_link'`.~~ **✅ DONE**
2. `mqtt-telemetry.service.ts` — добавить `sendSet(deviceToken, id,
   val)` и `sendInvoke(deviceToken, id, args?)`. Старые методы
   (`sendDryingCommand` etc.) — пометить deprecated, переписать как
   обёртки над `sendInvoke` через `getMenuIdByRole`.
3. `config.handler.ts` — парсить config (с `r:`),
   сохранять в `device_configurations`, эмитить `device:config` через
   WebSocket. **✅ DONE** (`persistMenuConfig` + socket emit реализованы).
4. ~~`devices.service.ts` — добавить `getMenuIdByRole(deviceId, role)`.~~ **✅ DONE**
5. `GET /devices/:id/menu-config` — кэшированное меню из БД. **✅ DONE**

### Шаг 5 — Frontend portal

#### 5a. Генерация canonical-roles.ts
Расширить `gen_ts_types.py`: выгружать `canonical_roles` из `mqtt_contract.yaml`
в `canonical-roles.ts`. Портал импортирует этот файл — не редактирует вручную.

```typescript
// canonical-roles.ts (autogenerated)
export const CANONICAL_ROLES = {
  "storage.led_brightness": { type: "uint", widget: "BrightnessSlider" },
  "drying.start":           { type: "action", widget: "StartButton" },
  // ...
} as const;
export type WidgetName = typeof CANONICAL_ROLES[keyof typeof CANONICAL_ROLES]["widget"];
```

#### 5b. Widget registry
Hand-written. TypeScript проверяет полноту при компиляции:

```typescript
// widget-registry.tsx
const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  BrightnessSlider: BrightnessSliderWidget,
  StartButton: StartButtonWidget,
  // пропустить → ошибка TypeScript при компиляции
};
```

Стандартные виджеты: `slider`, `number`, `toggle`, `select`, `button`.
Хардкод-виджеты: `ProfileEditor`, `RfidWriter`, `LedPulse`.

#### 5c. Dashboard-карточка — два режима

```typescript
const HARDCODED_CARDS: Partial<Record<DeviceType, React.ComponentType>> = {
  DRYER:        DryerCard,        // TODO: migrate to dynamic
  IHEATER_LINK: IHeaterCard,      // TODO: migrate to dynamic
};

function DeviceCard({ device }) {
  const HardcodedCard = HARDCODED_CARDS[device.deviceType];
  return HardcodedCard
    ? <HardcodedCard device={device} />
    : <DynamicDeviceCard device={device} />;
}
```

`Partial<Record<...>>` — явный сигнал что это переходное. Новые устройства
сразу идут в dynamic-режим.

#### 5d. DynamicDeviceCard
Получает menu-config устройства → фильтрует items с известным `r:` →
рендерит виджет из реестра. Группировка по `p:` сохраняет структуру
подменю (CHIPSET group, COLOR ORDER group и т.п.).

#### 5e. Пресеты
Переписать на роли (как в §7.2).

### Шаг 6 — Удалить устаревшее
1. После работающей миграции iHeater и Storage — удалить из контракта
   product_variants.iheater_link.extra_fields.
2. Удалить из SDK старые command-handlers (drying/storage/etc.) для
   продуктов в `menu_protocol_v1`.
3. RP2040 (сушилка) — отдельной задачей после согласования.

---

---

## 14. Что уже реализовано

### Backend (iDryerPortal/backend)
- **`mapDeviceType`**: `storage_link` → `DeviceType.STORAGE_LINK` ✅
- **Prisma**: enum `STORAGE_LINK` добавлен ✅
- **`persistMenuConfig`**: сохраняет menu JSON в `device_configurations` ✅
- **`GET /devices/:id/menu-config`**: кэшированное меню из БД ✅
- **`PUT /devices/:id/settings`**: `{cmd:set/invoke, id, val}` → MQTT ✅
- **`getMenuIdByRole(deviceId, role)`**: canonical role → menu item id ✅
- **WebSocket `device:config`**: эмит в комнату устройства при получении нового config ✅

### Frontend (iDryerPortal/frontend-v2)
- **`DeviceMenuPanel`**: отображает полное дерево меню устройства на странице `/devices/:id` ✅
- **`fetchDeviceMenuConfig`**: API-клиент для `/menu-config` ✅
- **Live-update**: подписка на `device:config` через socket, обновление без перезагрузки ✅
- **`.env.local`**: `VITE_WS_URL=ws://localhost:3000` (локальная разработка) ✅

### Firmware / SDK (idryer-core)
- **HA integration**: `ha_builder.cpp`, `ha_publisher.cpp` — discovery в `homeassistant/` топики, устройства видны в Home Assistant ✅
- **`r:` поле в menu items**: iHeater Link и Storage Link публикуют canonical roles ✅
- **`commands/set` + `commands/invoke`**: прошивка принимает и обрабатывает ✅

### Ещё не реализовано (остаток Этапа 1)
- `gen_ts_types.py` → `canonical-roles.ts` для портала
- `widget-registry.tsx` + `DynamicDeviceCard`
- `pre_gen_menu.py` валидация ролей против контракта
- Dashboard-карточки для iHeater Link и Storage Link (сейчас Storage показывает UI сушилки)

---

*Документ описывает целевую архитектуру. Для текущего состояния
обмена см. `mqtt_contract.yaml`. Решения по §10.2-10.4 приняты.
Этап 1 — в работе, см. §14.*

---

## 15. Стратегический план работ

> **Цель:** сделать систему универсальной — любое новое устройство
> автоматически получает меню настроек и карточку на дашборде без
> ручного кода на портале. Параллельно — очистить протокол команд
> от legacy-топиков.

**Термины, используемые в плане:**

- **Canonical role** (`r:`) — стандартное смысловое имя параметра или действия, например `drying.target_temperature`. Зафиксировано в `mqtt_contract.yaml`. Firmware и портал говорят на одном языке через эти имена.
- **Menu-as-protocol** — прошивка публикует полное дерево настроек устройства в MQTT-топике `idryer/{serial}/config`. Портал читает его и строит UI динамически, без хардкода.
- **Виджет** — React-компонент на портале, который отображает один параметр: `ToggleWidget`, `SliderWidget`, `ButtonWidget` и т.д.
- **Widget registry** — словарь `{ "toggle": ToggleWidget, "slider": SliderWidget, ... }`. Портал смотрит в него, чтобы по имени виджета из меню найти нужный компонент.
- **Dashboard-карточка** — плашка на главной странице портала (`/`). Показывает ключевые параметры устройства и быстрые действия.
- **`commands/set` / `commands/invoke`** — унифицированный MQTT-протокол команд. `set` — изменить значение параметра, `invoke` — вызвать действие (например, запустить сушку). Заменяет старые отдельные топики.
- **`gen_ts_types.py`** — скрипт, генерирующий TypeScript-типы из `mqtt_contract.yaml`. Запускается при сборке, результат коммитится в репозиторий.
- **`pre_gen_menu.py`** — скрипт проверки прошивки. Запускается при сборке прошивки и проверяет, что все `r:` в `menu.yaml` существуют в `mqtt_contract.yaml`. Если нет — сборка падает.

---

### Шаг 1 — Контракт как единственный источник правды ✅

`mqtt_contract.yaml` содержит секцию `canonical_roles` с описанием всех
стандартных параметров и действий. Сделано.

**Следующий микрошаг:** при добавлении нового устройства — сначала
дополнять `canonical_roles`, затем добавлять `r:` в `menu.yaml`.

---

### Шаг 2 — Проверка ролей при сборке прошивки

**Что делаем:** расширяем `pre_gen_menu.py` так, чтобы скрипт проверял
каждый `r:` в `menu.yaml` против списка ключей в `canonical_roles`.
Если роль неизвестна — сборка прошивки завершается с ошибкой.

**Зачем:** исключает ситуацию, когда прошивка публикует роль, которую
портал не понимает. Проблема обнаруживается на этапе сборки, а не в
продакшне.

**Файлы:** `idryer-core/scripts/pre_gen_menu.py`,
`idryer-core/contracts/mqtt_contract.yaml`

---

### Шаг 3 — TypeScript-типы canonical roles для портала

**Что делаем:** расширяем `gen_ts_types.py` — добавляем генерацию
файла `canonical-roles.ts`:

```typescript
// Авто-генерируется из mqtt_contract.yaml — не редактировать вручную
export const CanonicalRoles = {
  "drying.target_temperature": { type: "float", unit: "°C", widget: "slider" },
  "iheater.bambu_enabled":     { type: "bool",  widget: "toggle" },
  // ...
} as const;

export type CanonicalRole = keyof typeof CanonicalRoles;
```

**Зачем:** портал знает список допустимых ролей на уровне компилятора
TypeScript. Опечатка в имени роли → ошибка сборки фронтенда.

**Файлы:** `idryer-core/scripts/gen_ts_types.py`,
`idryer-core/contracts/_generated/canonical-roles.ts`

---

### Шаг 4 — Библиотека виджетов на портале

**Что делаем:** создаём `widget-registry.tsx` — словарь, связывающий
имя виджета из меню с React-компонентом. Ключи берутся из
`CanonicalRoles`, поэтому TypeScript проверяет полноту при сборке.

```typescript
// Тип гарантирует: каждый виджет из canonical_roles должен быть реализован
type WidgetName = typeof CanonicalRoles[CanonicalRole]["widget"];
const widgetRegistry: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  toggle:  ToggleWidget,
  slider:  SliderWidget,
  button:  ButtonWidget,
  hidden:  () => null,
};
```

Компоненты `ToggleWidget`, `SliderWidget`, `ButtonWidget` — минималистичные,
без device-специфичной логики. Знают только как отобразить значение и
отправить `commands/set` или `commands/invoke`.

**Файлы:** `frontend-v2/src/components/widgets/widget-registry.tsx`

---

### Шаг 5 — Динамическая dashboard-карточка

**Что делаем:** реализуем `DynamicDeviceCard` — компонент, который
получает menu-config устройства, фильтрует items с известными `r:` и
рендерит виджеты из реестра. Для известных устройств (DRYER,
IHEATER_LINK) временно остаются хардкодные карточки — они мигрируют
постепенно.

```typescript
// Переходная схема: хардкод → dynamic
const HARDCODED_CARDS: Partial<Record<DeviceType, React.ComponentType>> = {
  DRYER:        DryerCard,        // TODO: мигрировать на dynamic
  IHEATER_LINK: IHeaterCard,      // TODO: мигрировать на dynamic
};

function DeviceCard({ device }) {
  const HardcodedCard = HARDCODED_CARDS[device.deviceType];
  return HardcodedCard
    ? <HardcodedCard device={device} />
    : <DynamicDeviceCard device={device} />;
}
```

**Ближайшая цель:** Storage Link — первое устройство, которое сразу
идёт в dynamic-режим (сейчас показывается UI сушилки — это баг).

**Файлы:** `frontend-v2/src/components/DeviceCard.tsx`,
`frontend-v2/src/components/DynamicDeviceCard.tsx`

---

### Шаг 6 — Очистка протокола команд

**Контекст:** часть устройств (iHeater, Storage) до сих пор слушает
legacy MQTT-топики: `commands/link_integration`, `commands/bambu_apply`
и т.п. Новый протокол — единый `commands/set {id, val}` и
`commands/invoke {id}`.

**Что делаем:**
1. Убираем из прошивки обработчики legacy-топиков поочерёдно, начиная
   с iHeater Link и Storage Link.
2. На портале — убеждаемся, что все действия идут только через
   `PUT /devices/:id/settings` → `commands/set` / `commands/invoke`.
3. После миграции — удаляем legacy-поля из `mqtt_contract.yaml`.

**Порядок:** сначала iHeater Link (меньше legacy), затем Storage Link.
Сушилка (RP2040) — отдельная задача, после согласования с командой.

---

### Шаг 7 — Документация для разработчика

Короткий `CONTRIBUTING.md` в `idryer-core/contracts/`:

- Как добавить новое устройство (checklist: контракт → menu.yaml → прошивка → портал auto).
- Как добавить новый виджет (контракт → gen_ts_types → widget-registry → готово).
- Что такое canonical role и зачем она нужна.

**Зачем:** следующий разработчик не должен читать весь этот документ,
чтобы добавить кнопку.

---

### Шаг 8 — Миграция сушилки (отложено)

Сушилка (DRYER, RP2040) работает на старом протоколе. Миграция
затронет прошивку, портал и, возможно, существующих пользователей.
Выполняется отдельным этапом после стабилизации Шагов 1–7.

---

### Итоговый порядок

| # | Задача | Зависит от |
|---|--------|-----------|
| 1 | `pre_gen_menu.py` — валидация ролей | — |
| 2 | `gen_ts_types.py` → `canonical-roles.ts` | — |
| 3 | `widget-registry.tsx` | 2 |
| 4 | `DynamicDeviceCard` (Storage Link первый) | 3 |
| 5 | Очистка legacy-команд iHeater + Storage | 4 |
| 6 | CONTRIBUTING.md для разработчика | 1–5 |
| 7 | Миграция сушилки | 1–6, отдельно |

Шаги 1 и 2 независимы — можно делать параллельно.
