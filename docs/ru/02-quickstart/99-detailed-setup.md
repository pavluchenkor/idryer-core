# Углублённая настройка

Если вы здесь впервые — перейдите на [Запустить за 5 минут](01-five-minutes.md), эта страница для углублённой настройки и решения проблем.

Короткий путь: подключить библиотеку, прошить пример, увидеть мигающий LED и устройство в портале.

## Что подготовить

- Плата ESP32 (рекомендуемые: ESP32-C3 DevKit, Super Mini, XIAO ESP32-S3, Waveshare ESP32-S3 Zero).
- PlatformIO с framework `arduino`, platform `espressif32`.
- WiFi 2.4 GHz с доступом в интернет.
- Аккаунт в [portal.idryer.org](https://portal.idryer.org/) для claiming.

## Шаг 1. Подключить библиотеку

В `platformio.ini` своего продукта:

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; нужно только для mqtt_with_local_ws

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

## Шаг 2. Создать `secrets.h`

Скопируйте [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) в `include/secrets.h` своего проекта и пропишите свой SSID/пароль. Файл должен быть в `.gitignore`.

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

`IDRYER_API_BASE` обычно задаётся через `build_flags`, не через secrets.h.

## Шаг 3. Открыть первый пример

Самый простой — [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino). Скопируйте его как стартовую точку:

- Не требует датчиков, периферии и LAN WS.
- Не требует ручного `handleCommand` — встроенный fallback в `IdryerRuntime` обрабатывает базовые команды.
- LED моргает, когда устройство online — это и есть индикатор успеха.

## Шаг 4. Прошить и наблюдать

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

Ожидаемая последовательность в логе:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← ждём claim
[CSM] PIN: 1234567   expires in 600s          ← если auto-claim включён
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← готово, LED начинает моргать
[RT]  Cloud Online
```

## Шаг 5. Привязать устройство к аккаунту

Авто-claim уже включён в примере. PIN появляется в логе. Введите его в [portal.idryer.org](https://portal.idryer.org/) → "Add device". После claiming `CloudStateMachine` перейдёт в `Online`.

## Что дальше

Следующие примеры — каждый вводит одну новую сложность:

| Пример | Что добавляется |
|--------|-----------------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | свой `handleCommand`, обработка `commands/invoke` и `commands/set` |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | provisioning WiFi через Improv (без хардкода credentials) |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | локальный LAN WebSocket-сервер + `DevicePublisher` (один publish — два транспорта) |

## Dev REPL через Serial (без портала, без браузера)

Альтернативный путь для разработчика — увидеть полный claim flow глазами в обычном Serial-мониторе, без Improv и без UI портала.

В `platformio.ini` создайте dev-окружение с флагом `-DIDRYER_DEV_REPL=1`:

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Что включает флаг:
- HAL-логи в `Serial` идут **сразу** с момента boot (никакого молчания до WiFi-connect).
- Improv-провизионирование **отключено** — Serial свободен для интерактивных команд.
- В `main.cpp` появляется простой REPL: `wifi`, `claim`, `status`, `wipe`, `restart`, `help`.

Полный путь:

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

В мониторе:

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — введи в портал
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

REPL принимает команды независимо от настройки line-ending в Serial monitor (`\n`, `\r`, или idle-timeout 120 мс) — работает в любом терминале, включая `pio device monitor`, Arduino IDE Serial Monitor, `screen`, `picocom`.

Production-сборка (`-e my-device-prod`, без `IDRYER_DEV_REPL`) использует Improv через Chrome (`https://www.improv-wifi.com/`) и не содержит REPL-кода — флаг compile-time, экономит Flash.

`secrets.h` с `WIFI_SSID/WIFI_PASSWORD` (Шаг 2) остаётся отдельным путём для headless CI/auto-flash сценариев — работает в обоих environments.

После того как любой из примеров завёлся, читайте:

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — порядок объектов в `main.cpp`.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — как движутся данные.
- [04-patterns/](../04-patterns/) — рецепты: добавить sensor, peripheral, transport.
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — полный чеклист нового продукта.
- [10-troubleshooting.md](../10-troubleshooting.md) — что делать, если стек застрял.
