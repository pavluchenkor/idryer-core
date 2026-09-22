# Examples — точка входа

Три примера на `iDryer::Link`, по возрастанию сложности. Железа, кроме платы ESP32, не нужно.

| # | Пример | Что показывает |
|---|--------|----------------|
| 1 | [`01_blink_status`](01_blink_status/01_blink_status.ino) | минимальное устройство: телеметрия и состояние связи на LED |
| 2 | [`02_card_actions`](02_card_actions/02_card_actions.ino) | действия карточки: запуск с параметрами, стоп, вызов прибора; status и блок сессии |
| 3 | [`03_card_entities`](03_card_entities/03_card_entities.ino) | свои датчики и контролы на карточке: сенсор, кнопка, число, список, разметка |

В начале каждого `.ino` — блок «что показывает / что настроить / Common pitfalls».

## Проект PlatformIO

Ядро — в `lib/idryer-core` (копия, git-submodule или символическая ссылка). Пример копируется в `src/main.cpp`.

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP — транспорт ESP8266 из зависимостей espMqttClient: на ESP32
; не собирается.
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

Остальные библиотеки (MQTT, ArduinoJson, WebSockets, Improv) приходят из `library.json` ядра. Кавычки в строковых макросах нужны и снаружи, и внутри.

## Wi-Fi и привязка — не в коде

1. Прошейте пример: `pio run -t upload`.
2. Wi-Fi: приложение iDryer передаёт сеть по воздуху (ESPTouch), или веб-установщик — по USB (Improv). Сеть только 2,4 ГГц.
3. Привязка: приложение находит устройство в локальной сети и передаёт одноразовый токен. После этого устройство появляется в аккаунте на портале и в приложении.

Отвязка в приложении или на портале приходит командой `revoke`: ядро стирает секрет, и устройство снова ждёт привязки.

Лог: `pio device monitor -b 115200`.

## Что читать дальше

- [`docs/ru/09-add-product/01-add-new-product.md`](../docs/ru/09-add-product/01-add-new-product.md) — новый продукт на Link целиком.
- [`docs/ru/09-add-product/02-add-widget.md`](../docs/ru/09-add-product/02-add-widget.md) — карточка устройства: card-манифест, действия, параметры из меню.
- [`docs/ru/03-public-api/01-link-api-reference.md`](../docs/ru/03-public-api/01-link-api-reference.md) — справочник API `iDryer::Link`.
- [`docs/ru/10-troubleshooting/`](../docs/ru/10-troubleshooting/) — типовые проблемы.
