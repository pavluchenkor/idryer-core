# Подробная настройка

Короткий путь — [Запустить за 5 минут](01-five-minutes.md). Здесь — окружение, флаги сборки, логи и режим разработки.

## Ядро в проекте

Ядро лежит в `lib/idryer-core` проекта PlatformIO: копией, git submodule или символической ссылкой на общий клон. Его `library.json` приносит зависимости: MQTT, ArduinoJson, WebSockets, Improv. В `lib_deps` проекта — только библиотеки ваших датчиков.

Платы продуктов на ядре: ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP — транспорт ESP8266 из зависимостей espMqttClient: на ESP32 не собирается.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Плата без USB-UART (ESP32-C3 SuperMini): Serial — по USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Флаг | Зачем |
|---|---|
| `IDRYER_API_BASE` | адрес API портала: активация, привязка |
| `MQTT_BROKER`, `MQTT_PORT` | брокер портала |
| `MQTT_USE_TLS=1` | защищённое соединение с брокером |
| `lib_ignore = ESPAsyncTCP` | транспорт ESP8266 из зависимостей MQTT-клиента: на ESP32 не собирается |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial по USB у плат без USB-UART |

Кавычки в строковых макросах нужны и снаружи, и внутри.

## Периоды публикации

Поля `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs`; ноль — значение из контракта:

| Что | В работе | В простое |
|---|---|---|
| телеметрия | 30 с | 60 с |
| статус | сразу при смене режима, уставки или времени; сверка раз в 60 с | сверка раз в 5 мин |

«В простое» — ни один юнит не в активном режиме.

## Логи

```bash
pio device monitor -b 115200
```

Пока у устройства нет сети, лог молчит: порт занят Improv. Логи включаются строкой `[BOOT] WiFi ok, logs enabled`. После этого работают команды `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` — см. [Привязка к аккаунту](02-claim.md).

## Режим разработки: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

С флагом:

- логи идут в порт сразу после включения;
- Improv и команды `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` выключены — входящие строки порта читает ваш код;
- сеть передаёт приложение (ESPTouch) или код: `seedWifiCredentialsIfEmpty()` до `begin()` — см. [Wi-Fi](01-wifi.md).

Выпускаемая прошивка собирается без флага.

## Что дальше

- [Примеры ядра](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Как добавить новый продукт](../09-add-product/01-add-new-product.md).
- [Карточка устройства: card-манифест](../09-add-product/02-add-widget.md).
