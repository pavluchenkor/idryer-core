# Поток данных

Описание движения данных в работающем устройстве. Цель — показать, что `idryer-core` не использует ни event bus, ни service locator: участники соединяются явными указателями в composition root, и каждое направление потока — отдельный, читаемый путь.

Подробные паттерны "как пробросить данные между моими частями" — в [12-patterns/04-data-flow.md](../12-patterns/04-data-flow.md).

## Основные направления

```
                Backend / app
                     │
                     │ MQTT commands/*
                     ▼
        ┌──────────────────────────────┐
        │  MqttClient                  │
        │  parses topic + payload      │
        └──────────────┬───────────────┘
                       │
                       │ CommandCallback
                       ▼
        ┌──────────────────────────────┐
        │  IdryerRuntime               │
        │  ping → settimeofday + info  │
        │  others → CommandHandler     │
        └──────────────┬───────────────┘
                       │
                       │ commandHandler_(cmd, data)
                       ▼
        ┌──────────────────────────────┐
        │  Product handleCommand()     │
        │  invoke / set / get_config / │
        │  product-specific commands   │
        └──────┬───────────────┬───────┘
               │               │
               ▼               ▼
   ActionDispatcher        IProfile             Sensor / Actuator TODO:
   handleInvoke / Set      getConfig            (product code)
                           applyConfig
                           buildInfoJson
```

```
       Sensor (product)            Profile / executor
            │                           │
            │ tick() / read             │ updates state
            ▼                           ▼
       ┌───────────────────────────────────────┐
       │  Product Publisher                    │
       │  (StorageTelemetryPublisher, …)       │
       │  builds JsonDocument                  │
       └────────────────┬──────────────────────┘
                        │
                        │ pub.publishX(doc)
                        ▼
       ┌───────────────────────────────────────┐
       │  DevicePublisher (optional)           │
       │  dual-publish helper: MQTT + Local WS │
       └─────────┬─────────────────────┬───────┘
                 │                     │
                 ▼                     ▼
            MqttClient            LocalAccess (WS)
            broker                LAN client
```

## Входящие команды

1. **MQTT** доставляет сообщение в топике `idryer/{serial}/commands/{cmd}`.
2. `MqttClient::handleMessage` парсит payload как JSON и вызывает `CommandCallback`.
3. `CommandCallback` зарегистрирован `IdryerRuntime` в `begin()` — он принимает `(command, data)`, где `command` — суффикс после `commands/`.
4. `IdryerRuntime::onMqttCommand`:
   - Если `command == "ping"` — синхронизирует время и публикует info. Не передаёт дальше.
   - Если зарегистрирован `commandHandler_` — передаёт всё остальное в продукт.
   - Иначе — запасной встроенный путь: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `device.getConfig` → `IProfile::getConfig`.

5. **Local WS** (если используется) принимает `{"type":"command","command":"...","data":{...}}`, разворачивает envelope и вызывает тот же `CommandSink`, что зарегистрирован у MQTT-пути. Один обработчик — два транспорта.

## Исходящие данные

Библиотека не публикует ничего, что её не попросили. Все исходящие сообщения инициирует продукт:

| Что | Кто инициирует | Через какой API |
|-----|---------------|-----------------|
| `info` | `IdryerRuntime` (один раз при `Online` и при `ping`) | `MqttClient::publishInfoJson` |
| `telemetry` | продуктовый publisher | `MqttClient::publishTelemetry` или `DevicePublisher::publishTelemetry` |
| `status` | продуктовый код при изменении состояния | `MqttClient::publishStatus` или `DevicePublisher::publishStatus` |
| `config` | `handleCommand` при `device.getConfig` или `get_config` | `MqttClient::publishConfig` |
| `events` | продуктовый код при событии | `MqttClient::publishEvent` |
| `integrations/status` | `LinkIntegrationsManager` | `MqttClient::publishIntegrationsStatus` |
| `offline` | брокер автоматически (LWT) | устройство не публикует |

## Связи между объектами в composition root

Ссылки между участниками передаются явно через конструкторы и сеттеры. Никаких глобальных реестров.

```
ArduinoWifiManager     ─┐
ArduinoCredentialStore ─┤
HttpApi (← Http)       ─┼──→ CloudStateMachine ──→ IdryerRuntime ──→ MqttClient
MqttClient             ─┘                              ▲
                                                       │
                                ActionDispatcher ──────┤
                                IProfile         ──────┘

                LocalAccess  ──── (setCommandSink) ────→ same handleCommand
                DevicePublisher (&MqttClient, &LocalAccess)

                Sensor  ──→ Publisher  ──→ DevicePublisher  ──→ MqttClient + LocalAccess
                Executor ←── ActionDispatcher (invoke)  ←── handleCommand
```

Каждое соединение — одна строка в `main.cpp`. Это и есть "explicit composition root".

## Почему так

- **Никакой магии**: чтобы понять, как данные попадают из датчика в облако, читатель видит цепочку указателей в `main.cpp`. Ни один поток данных не скрыт за фасадом.
- **Гибкость**: продукт выбирает, использовать ли `DevicePublisher` (MQTT + WS), или публиковать только в MQTT, или вовсе свой собственный publisher с дополнительной логикой.
- **Тестируемость**: каждый узел — отдельный класс с явными зависимостями. Можно подменять моками без изменения остального стека.

## Чего здесь нет специально

- Нет глобального event bus или message broker внутри устройства.
- Нет автоматического обнаружения "у меня есть датчик, опубликую его данные сам".
- Нет реестра типов "device знает все свои telemetry-поставщики".

Если такие связи нужны продукту — он добавляет их в свой product code. Библиотека их не навязывает.
