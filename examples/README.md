# Examples — точка входа

Четыре примера, выстроенных по возрастанию сложности. Запускайте по порядку.

## Порядок запуска

| # | Пример | Что показывает |
|---|--------|----------------|
| 1 | [`01_blink_status`](01_blink_status/01_blink_status.ino) | минимальный composition root; LED моргает, когда устройство онлайн |
| 2 | [`minimal_mqtt_only`](minimal_mqtt_only/minimal_mqtt_only.ino) | свой `handleCommand`, обработка `commands/invoke` и `commands/set` через `ActionDispatcher` |
| 3 | [`03_with_improv`](03_with_improv/03_with_improv.ino) | provisioning WiFi через Improv (без хардкода SSID/пароля) |
| 4 | [`mqtt_with_local_ws`](mqtt_with_local_ws/mqtt_with_local_ws.ino) | LAN WebSocket-сервер + `DevicePublisher` (один publish — два транспорта) |

В начале каждого `.ino` есть блок «что показывает / что обязательно настроить / Common pitfalls».

## Что подготовить до запуска

### 1. `include/secrets.h`

Скопируйте [`secrets.h.example`](secrets.h.example) в `include/secrets.h` своего PlatformIO-проекта и пропишите свои значения:

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

Файл должен быть в `.gitignore`. Пример `03_with_improv` `WIFI_SSID`/`WIFI_PASSWORD` не использует, остальные три — используют.

### 2. `build_flags` в `platformio.ini`

```ini
build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

`IDRYER_API_BASE` — адрес provisioning-API. Кавычки нужны и снаружи (для Make), и внутри (для C-препроцессора). Для staging: `https://staging.idryer.org/api`.

### 3. WiFi 2.4 GHz

ESP32 не работает с 5 GHz сетями. Проверьте, что точка доступа выдаёт 2.4 GHz и не блокирует исходящие соединения на порты 443 (HTTPS) и 8883 (MQTT TLS).

### 4. Где смотреть лог

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

Ожидаемая последовательность для любого из примеров:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
[CSM] PIN: 1234567   expires in 600s
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online
[RT]  Cloud Online
```

PIN введите в [portal.idryer.org](https://portal.idryer.org/) → "Add device". После claiming устройство переходит в `Online`.

## Если что-то не работает

См. [`docs/ru/10-troubleshooting.md`](../docs/ru/10-troubleshooting.md) — типовые проблемы по разделам: WiFi, provisioning, MQTT, команды, telemetry, NTP, ArduinoJson, Improv, LocalAccess.

## Что читать дальше

- [`docs/ru/02-getting-started.md`](../docs/ru/02-getting-started.md) — те же шаги, расширенно.
- [`docs/ru/02-architecture/01-composition-root.md`](../docs/ru/02-architecture/01-composition-root.md) — порядок объектов в `main.cpp`.
- [`docs/ru/02-architecture/03-data-flow.md`](../docs/ru/02-architecture/03-data-flow.md) — поток данных в работающем устройстве.
- [`docs/ru/12-patterns/`](../docs/ru/12-patterns/) — рецепты: добавить sensor, actuator, transport.
- [`docs/ru/10-how-to-add-product/01-add-new-product.md`](../docs/ru/10-how-to-add-product/01-add-new-product.md) — чеклист сборки нового продукта.
