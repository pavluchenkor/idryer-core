---
title: "Как добавить новый продукт на базе idryer-core"
description: "Чеклист нового устройства iDryer на фасаде iDryer::Link: проект, минимальная прошивка, Wi-Fi и привязка в приложении, телеметрия, меню настроек, карточка устройства, контракт."
---

# Как добавить новый продукт на базе idryer-core

Эта инструкция нужна, когда вы делаете новый продукт на базе `idryer-core`: сушилку филамента, нагревательный блок, подсветку, датчик или другой модуль. Она показывает, что ядро делает за вас и что должен добавить код продукта.

Полный собираемый пример — нагреваемый шкаф хранения из документации Build-Your-Own-iDryer (`example/09-cabinet`): в нём пройдено всё, что есть на этой странице.

---

## На чём строится продукт

Продукт общается с ядром через один объект — фасад `iDryer::Link` (`<iDryer.h>`). Внутри `s_link.begin()` и `s_link.loop()` ядро:

- получает сеть Wi-Fi из приложения iDryer по воздуху (ESPTouch) или из веб-установщика по USB (Improv) и держит подключение;
- привязывает устройство к аккаунту: ждёт одноразовый токен привязки — из приложения по локальной сети или через последовательный порт (`PAIR_TOKEN:<токен>`) — и обменивает его на портале на постоянный секрет;
- подключается к MQTT и публикует `telemetry` и `status` с периодами из `Config`;
- объявляет себя в локальной сети (mDNS `_idryer._tcp`) и принимает команды приложения по WebSocket;
- публикует card-манифест карточки устройства.

Классы нижнего уровня (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` и другие) — внутренности ядра: токен привязки доходит до облачной части только внутри `iDryer::Link`. Стройте продукт на фасаде.

---

## 1. Проект

`idryer-core` кладётся в `lib/idryer-core/` (копия или символическая ссылка); библиотеки ядра PlatformIO берёт из его `library.json`. Минимальный `platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; транспорт ESP8266 из зависимостей espMqttClient: на ESP32 не собирается
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Без `lib_ignore = ESPAsyncTCP` сборка падает в `ESPAsyncTCP.cpp`; без `MQTT_BROKER` и `MQTT_PORT` ядро не компилируется.

---

## 2. Минимальная прошивка

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // не продукт iDryer: карточка — из манифеста
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // Устройство отвязали на портале: стереть секрет, ждать новой привязки.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // ваши функции датчиков
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` запускает Wi-Fi, привязку, MQTT и локальный доступ; `s_link.loop()` должен крутиться постоянно, без `delay()`. Команда `revoke` приходит с портала, когда устройство отвязывают от аккаунта: `handleRevoke()` стирает секрет, и устройство ждёт нового токена привязки.

---

## 3. Wi-Fi и привязка — не в коде

В прошивке нет ни пароля от сети, ни данных аккаунта. Пользователь подключает устройство в приложении iDryer: **Подключить новое устройство** → шаг **Wi-Fi** (приложение передаёт сеть через ESPTouch) → шаг **Привязка** (приложение находит устройство через mDNS, получает у портала одноразовый токен и передаёт его устройству; токен на портале устройство активирует само). Пока Wi-Fi не поднялся, последовательный порт молчит: ядро держит его для веб-установщика (Improv).

По шагам и с ожидаемым логом — Build-Your-Own-iDryer, глава «Старт прошивки на ядре».

---

## 4. Данные: телеметрия и статус

- Флаги `has*` в `Config` определяют, какие словарные поля уходят в телеметрию и какие ячейки появятся на карточке.
- Значения пишите в `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) и `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); ядро публикует их с периодами из `Config`, а `s_link.publishStatusNow()` отправляет статус сразу.
- Своё поле — через `s_link.onTelemetryPublish()`, на карточку — через `s_link.card().sensor()`; см. [Карточка устройства](02-add-widget.md).

---

## 5. Настройки: меню

Настройки описываются в `src/menu/menu.yaml`; генератор `menu_gen.py` делает из него C++-код, хранение в NVS и JSON меню ([Меню как протокол](../08-contracts/02-menu-as-protocol.md)). Код продукта:

- загружает меню до `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- публикует его через `menu_buildFullJson()` и `s_link.devicePublisher()->publishConfigRaw()` — при выходе в онлайн и по команде `get_config`;
- применяет команду `set` через `menu_apply_by_bind()` (значение, NVS и кэш разом) и публикует меню заново.

Полный код — Build-Your-Own-iDryer, глава «Меню из YAML».

---

## 6. Карточка устройства

Что показывает карточка и какие операции запускает, объявляется через `s_link.card()` — [Карточка устройства: card-манифест](02-add-widget.md).

---

## 7. Контракт

Когда добавляете новые топики или меняете payload:

1. обновите `contracts/mqtt_contract.yaml`;
2. запустите `contracts/regen.sh` и закоммитьте сгенерированные файлы.

---

## Двухчиповые устройства

Для ESP32, который работает с отдельным контроллером (например, RP2040) по UART, в ядре есть UART-мост `idryer_uart.h`; рабочий образец — прошивка `idryer-link`.
