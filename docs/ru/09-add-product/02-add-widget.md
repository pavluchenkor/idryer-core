---
title: "Карточка устройства: card-манифест"
description: "Как прошивка на idryer-core описывает свою карточку через s_link.card(): сенсоры, действия с параметрами запуска, что уходит на портал и что рисуют портал и приложение."
---

# Карточка устройства: card-манифест

Портал и мобильное приложение строят карточку любого устройства из его **card-манифеста** — описания, которое прошивка публикует о себе: что показать и чем можно управлять. Новому типу устройства не нужен код ни на портале, ни в приложении.

Манифест — часть фасада `iDryer::Link` (`s_link.card()`). Низкоуровневый runtime (`IdryerRuntime`) его не публикует.

Меню и карточка — разные вещи. Меню — зеркало настроек прибора: значение, изменённое с портала, записывается в память устройства. Карточка показывает измерения и запускает операции: параметры запуска уходят одной командой и в меню не пишутся. Параметр запуска может взять пределы и значение по умолчанию из пункта меню.

---

## Как это работает

```text
прошивка: объявления s_link.card()
   │  ядро собирает JSON и публикует его retained (QoS 1) в idryer/{key}/card
   ▼
бэкенд портала: проверяет манифест (лимиты, допустимые типы и поля), сохраняет
   ▼
портал и приложение: рисуют карточку
   │  пользователь нажимает кнопку
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
ядро передаёт "card.<id>" в ваш колбэк
```

Ядро публикует манифест после подключения к MQTT и перепубликует, когда меняется декларация или пункт меню, от которого она зависит.

---

## Сущности: что показать

Сенсоры из словаря экосистемы добавляются по флагам `Config`, объявлять их не нужно:

| Флаг `Config` | Ячейка карточки |
|---|---|
| `hasAirTemp` | температура воздуха |
| `hasAirHumidity` | влажность |
| `hasHeaterTemp` | температура нагревателя |
| `hasHeater` | мощность нагрева |
| `hasFan` | вентилятор вкл / выкл |
| `hasServo` | заслонка открыта / закрыта |
| `hasWeight` | весовые модули (топик `weights`) |
| `hasRfid` | у камеры есть RFID-ридер |

Своё значение: добавьте его в телеметрию и объявите сенсор с JSON-путём:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Простые контролы — тоже сущности: `button`, `number`, `select`. Значение уходит сразу, как пользователь его изменил, ядро вызывает ваш колбэк:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Действия: операции с параметрами запуска

Действие — операция прибора: начать сушку, нагреть, включить подсветку, остановить. У него есть **режим** — режим юнита после действия (`status.units[].mode`) — и **параметры** со смыслом (`purpose`). Что показать, карточка решает по текущему режиму юнита:

- режим юнита совпал с режимом действия → прибор занят им: блок сессии и кнопка «Стоп» (действие с режимом `IDLE`);
- иначе → форма запуска; действий запуска несколько → переключатель режимов.

Пример: нагреваемый шкаф хранения на одном ESP32. Целевая температура — пункт меню `target_temp` (30–50 °C, по умолчанию 45).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // уже в пределах 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // значения меню — в кэш, из него читает карточка
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

Что получает колбэк:

- числа зажаты в пределы юнита, отсутствующее число заменено значением по умолчанию;
- `unit` — индекс юнита из `unitId` (`"U1"` → 0); команда для юнита, которого у прибора нет, игнорируется;
- после запуска выставьте `status.mode` и вызовите `publishStatusNow()` — по статусу карточка переключится на блок сессии.

### API действий

| Вызов | Что делает |
|---|---|
| `card.action(id, mode, cb)` | действие; `mode` — режим юнита после него, `nullptr` — режим не меняется |
| `.param(id, purpose, MENU_ID)` | число: пределы, шаг, значение по умолчанию и единица — из пункта меню |
| `.param(id, purpose, min, max, step, def[, unit])` | число со своими пределами |
| `.ceiling(MENU_ID)` | верхний предел предыдущего параметра — значение этого пункта меню (например, максимальная температура воздуха) |
| `.stages(id, "stages", MENU_ID)` | стадии профиля `[{temperature, ramp, hold}]`, секунды; температура стадии — в пределах пункта меню |
| `.select(id, purpose, options, count[, def])` | выбор из списка; значение не из списка заменяется значением по умолчанию |
| `.color(id, purpose, "#FFFFFF")` | цвет `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | постоянные кнопки шапки: вызов прибора, сброс ошибок |
| `.name("ru", "…").name("en", "…")` | название действия, если карточке незнаком его режим |

Значения `purpose`: `target_temperature`, `target_humidity`, `duration` (минуты, 0 — без ограничения), `stages`, `start_stage` (с 0), `effect`, `rgb_color`. По ним карточка подписывает поля и подключает возможности портала: пресет сушки заполняет `target_temperature` и `duration` у действия с режимом `DRYING`, профиль сушки — `stages`.

Колбэки — функции или лямбды без захвата. Строки `name` и варианты выбора хранятся указателями — нужны литералы или статические массивы.

---

## Что уходит на портал

Шкаф из примера публикует (`Config`: `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`):

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` и `default` пришли из пункта меню. С `.ceiling()` у параметра появляется ещё `max_by` — заголовок пункта меню, который срезал верхний предел; карточка называет его, когда значение выше предела.

---

## Что рисуют портал и приложение

Схема, не скриншот. Ожидание:

```text
┌─ DIY Storage Cabinet ────────────── [Ожидание] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                   │  ← температура, влажность, нагреватель
│ | 0 %     | выкл            |                   │  ← мощность, вентилятор (серые: 0 / выкл)
│ [Темп. 45 °C          ]  [Хранение]             │  ← действие "storage"
└─────────────────────────────────────────────────┘
```

После «Хранение» прибор сообщает режим `STORAGE`, и та же карточка показывает блок сессии (цель и сколько длится хранение) и кнопку «Стоп» — действие с режимом `IDLE`.

Правила с обеих сторон:

- ячейка серая, когда значения нет; мощность — ещё и при 0; вентилятор и заслонка — в состоянии выкл / закрыта;
- значение вне пределов не подменяется: запуск блокируется, под формой — причина;
- у прошивки с действиями в манифесте кнопки вызова и сброса ошибок есть, только если она объявила действия с `identify` / `clear_errors`.

Где появляется карточка:

| | Портал | Приложение |
|---|---|---|
| тип устройства — не продукт iDryer | карточка дашборда из манифеста, действия на ней; страница устройства — показания и действия (если в манифесте есть действия) | главная — показания; действия — на странице устройства |
| `deviceType = Dryer` | карточка сушилки с теми же действиями из манифеста | главная — мониторинг; действия — на странице устройства |

---

## Ограничения

| | Ядро | Бэкенд портала |
|---|---|---|
| сущностей | 16 объявленных + автоматические | 32 |
| рядов layout | 8 | 16 |
| id в ряду | 4 | 4 |
| действий | 8 | 8 |
| параметров у действия | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| языков `name` | `ru`, `en` | до 4 |

`id`: строчные латинские буквы, цифры и `_`, до 24 символов; у сущностей и действий общее пространство id. Документ манифеста ограничен 4096 байтами; если не помещается, ядро пишет в лог `manifest overflows` и не публикует его.

Сам формат — `contracts/mqtt_contract.yaml`, раздел `mqtt_only`, `suffix: card`.
