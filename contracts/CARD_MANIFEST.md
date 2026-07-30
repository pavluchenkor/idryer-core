# Card Manifest — динамическая карточка устройства (спека для рендереров)

Аудитория: разработчики потребителей манифеста — портал (frontend-v2), **мобильное
приложение (Flutter)**, позже генератор HA discovery. Взгляд со стороны устройства —
Build-Your-Own-iDryer, раздел 10 («Умный фильтр»), глава 06-card.

Источник правды по wire-формату: `mqtt_contract.yaml → mqtt_only[suffix=card]`.
Этот файл — производная «инструкция рендерера»; при расхождении прав yaml.

---

## 1. Зачем

Устройство (в т.ч. стороннее, `deviceType=UNKNOWN`) публикует **entity manifest** —
самоописание «что показать и чем управлять». Потребители строят карточку из
манифеста автоматически: ни портал, ни приложение не знают тип устройства заранее.
Слой 1 — авто-список сущностей; слой 2 — заводская разметка `layout` из манифеста.

## 2. Транспорт до приложения

Приложение НЕ читает MQTT. Цепочка:

```
устройство → MQTT idryer/{serial}/card (retained, QoS1)
          → backend card.handler.ts (санитизация) → Device.cardManifest (Prisma)
          → REST GET /devices / /devices/{id} → поле device.cardManifest
          → при изменении манифеста backend шлёт WS device:refresh → refetch
```

Т.е. для Flutter: `cardManifest` — обычное поле device-ответа, обновление — по
`device:refresh` (механизм в приложении уже есть).

## 3. Схема (после санитизации backend — гарантии для рендерера)

```jsonc
{
  "v": 1,
  "entities": [ /* 1..32, id уникальны */ ],
  "layout":   [ /* опционально, ≤16 рядов по ≤4 id, только существующие id */ ]
}
```

Сущность (гарантированные инварианты — backend уже отфильтровал мусор):

| Поле | Тип/формат | Когда есть |
|---|---|---|
| `id` | `[a-z0-9_]{1,24}` | всегда, уникален |
| `type` | `sensor \| binary_sensor \| number \| select \| button` | всегда |
| `device_class` | `[a-z0-9_]` | опц. — семантика словарного сенсора |
| `label` | printable ASCII ≤32 | опц. (у словарных обычно нет — переводит потребитель) |
| `unit` | ASCII ≤8 (+°²³) | опц. |
| `source` | `telemetry \| status` | у sensor/binary_sensor всегда |
| `path` | JSON-путь ≤48, напр. `units[0].vocIndex` | у sensor/binary_sensor всегда |
| `action` | `[a-z0-9_.]{1,32}`, обычно `card.{id}` | у number/select/button всегда |
| `arg` | имя аргумента, дефолт `value` | у number/select |
| `min`/`max`/`step` | числа, min≤max | у number |
| `options` | 1..8 строк ASCII ≤32 | у select, непустой |

## 4. Алгоритм рендера (эталон — frontend-v2)

Эталонная реализация: `frontend-v2/src/components/device/entity/entity-manifest.ts`
(маппинг манифест→сущности) и `EntityDeviceCard.tsx` (рендер). Flutter зеркалит
логику, вёрстка своя.

### 4.1 Маппинг типов на виджеты

- `sensor` со словарным `device_class` (`temperature`, `humidity`, `heater_temp`,
  `power`) → штатная ячейка телеметрии с локализованной подписью и иконкой
  потребителя (label из манифеста игнорируется в пользу перевода);
- `binary_sensor` со словарным `device_class` (`fan`, `servo`) → штатная
  двоичная ячейка (Вкл/Выкл, Открыта/Закрыта);
- `sensor`/`binary_sensor` без словарного класса → генерик-ячейка: `label ?? id`,
  `unit`, значение по `path`, «—» пока данных нет;
- `button` → кнопка; `number` → поле с min/max/step + отправка; `select` →
  выпадающий список опций (отправка по выбору).

### 4.2 Слой 1 (нет layout): авто-раскладка

- порядок = порядок `entities[]`;
- подряд идущие ячейки-сенсоры группируются в ряды **максимум по 3**
  (4-я переносится);
- контролы (button/number/select) — каждый на всю ширину, разрывают ряд ячеек.

### 4.3 Слой 2 (есть layout)

- каждый ряд `layout` → горизонтальный ряд, ширина делится поровну между id;
- сущности, не упомянутые в layout, дорисовываются ниже авто-правилом 4.2
  (ничего не теряем).

## 5. Живые значения (path → данные)

- Снапшот: REST-ответ юнита (`temperature`, `humidity`, `fanStatus`…) для
  словарных классов.
- Live: WS `telemetry:update` `{deviceId, unitId, data}` — `data` содержит
  словарные поля (`temperature`, `humidity`, `heaterTemp`, `heaterPower`,
  `fanStatus`, `servoOpen`, `weights`, `rssi`) **и кастомные примитивы юнита
  как есть** (backend пробрасывает ≤16 number/boolean полей, напр. `vocIndex`).
- Резолв `path` для WS-данных: данные события уже пер-юнитные и плоские, поэтому
  берём **последний сегмент** пути: `units[0].vocIndex` → ключ `vocIndex`.
  (Полный JSON-путь нужен только консьюмерам сырого MQTT, напр. HA-генератору.)

## 6. Команды контролов

Нажатие/ввод → штатный командный канал (у приложения уже есть):
invoke с payload `{action: entity.action, args: {[entity.arg ?? "value"]: v}, unitId}`.
У button `args` пустой. Дальше DeviceCommandBus/watchdog как у всех команд.

**Правило проекта (незыблемое): НИКАКОГО optimistic UI.** Состояние на карточке
меняется только по подтверждённым данным устройства (telemetry/status/WS).
Нажали → команда ушла → ждём фактическое значение. Draft-значения контролов
(набранное в number до отправки, выбранное в select) — локальные, это параметры
команды, не состояние устройства.

## 7. Fallback

Если `cardManifest` пуст/отсутствует:
- известный `deviceType` → продуктовая карточка (как сейчас);
- `UNKNOWN` без манифеста → минимальная авто-карточка из
  `hardwareConfig[].capabilities` (стаб: `buildStubManifest` в entity-manifest.ts;
  словарь ключей canonical: `heater, fan, servo, led, weight, rfid, air_temp,
  air_humidity, heater_temp` + легаси-имена `TempAirSensor/RhAirSensor/…`).

## 8. Безопасность/устойчивость рендерера

Backend санитизирует, но рендерер обязан быть терпим: неизвестный `type` или
`device_class` → пропустить сущность (не падать); отсутствующее значение → «—»;
label рендерить как plain text (никакого HTML/markdown).

## 9. Референсы

- Схема: `mqtt_contract.yaml` → `mqtt_only[suffix=card]`
- SDK устройства: `idryer-core/src/card/card_builder.{h,cpp}` (авто-сенсоры из
  Config.has*, `card.{id}` роутинг invoke)
- Backend: `iDryerPortal/backend/src/mqtt-telemetry/handlers/card.handler.ts`
  (санитизация), `telemetry.handler.ts` (проброс кастомных полей в WS)
- Frontend-эталон: `iDryerPortal/frontend-v2/src/components/device/entity/`
- Туториал: `Build-Your-Own-iDryer/docs/ru/10-build-a-filter/` (гл. 5–7)
