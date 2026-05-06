# MQTT-топики и сообщения

Все топики имеют форму `idryer/{serial}/{suffix}`, где `{serial}` — серийный номер устройства.

Этот документ описывает топики и команды, которые реализует `MqttClient` из `idryer-core`. Полный интерфейс платформы (все команды от бэкенда для всех типов устройств) находится в `contracts/portal_backend_status.md`.

## Устройство → бэкенд

### info

```
idryer/{serial}/info    retained=true    QoS публикации=0
```

Публикуется один раз при первом выходе в Online, и повторно при получении команды `ping`.

Payload определяется продуктом через `IProfile::buildInfoJson()`. Минимально ожидаемые бэкендом поля: `hardwareVersion`, `firmwareVersion`, `timestamp`.

Пример для Storage Link:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

### telemetry

```
idryer/{serial}/telemetry    retained=false    интервал ~10 с
```

Публикуется продуктом через `pub.publishTelemetry()`. Библиотека не публикует автоматически.

Пример для Storage Link (климатический датчик):

```json
{
  "units": [
    {"unitId": "U1", "temperature": 23.5, "humidity": 47.2}
  ]
}
```

### status

```
idryer/{serial}/status    retained=true    публикуется по изменению
```

Публикуется продуктом при изменении состояния через `pub.publishStatus()`. Payload определяется продуктом.

### config

```
idryer/{serial}/config    retained=false    по запросу
```

Публикуется при получении команды `device.getConfig` (invoke) или при ответе на `get_config`. Вызывается через `pub.publishConfig()` или `pub.publishConfigRaw()`.

Для большого payload (> 16000 байт) публикуется чанками: каждый чанк содержит `tid`, `idx`, `total`, `last`, `d`.

### config/delta

```
idryer/{serial}/config/delta    retained=false    по изменению
```

Частичное обновление конфига через `pub.publishConfigDelta()`. Бэкенд ожидает поле `d` (объект с изменениями).

### events

```
idryer/{serial}/events    retained=false    по событию
```

Публикуется продуктом через `pub.publishEvent()`. Библиотека события не генерирует автоматически.

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    по изменению
```

Публикуется `LinkIntegrationsManager`. Содержит состояние активной интеграции.

### offline (LWT)

```
idryer/{serial}/offline    retained=false    при неожиданном отключении
```

Устанавливается брокером автоматически при разрыве TCP-соединения. Устройство никогда не публикует этот топик вручную.

## Бэкенд → устройство

Устройство подписывается на `idryer/{serial}/commands/#`.

### commands/ping

```
idryer/{serial}/commands/ping
```

Обрабатывается `IdryerRuntime` напрямую — синхронизирует системное время через `settimeofday()` и повторно публикует info.

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

Предпочтительный путь для продуктовых действий. Библиотека передаёт команду в продуктовый `CommandHandler` (рекомендуемый путь). Если `CommandHandler` не зарегистрирован, команда попадает во встроенный `ActionDispatcher` (fallback).

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

Встроенное действие `device.getConfig` обрабатывается рантаймом или продуктовым handler'ом — вызывает `IProfile::getConfig()` и публикует результат.

### commands/set

```
idryer/{serial}/commands/set
```

Установка одного параметра конфигурации. Передаётся в продуктовый `CommandHandler` (рекомендуемый путь). Fallback — встроенный `ActionDispatcher::handleSet()`, если `CommandHandler` не зарегистрирован.

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

Управление интеграциями. Обрабатывается `LinkIntegrationsManager` через продуктовый `CommandHandler`.

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```

### commands/bambu_apply

```
idryer/{serial}/commands/bambu_apply
```

Применение параметров филамента к AMS-слоту принтера Bambu. Обрабатывается `LinkIntegrationsManager`.

### Прочие команды платформы

Команды `drying`, `storage`, `profile`, `stop`, `get_config`, `read_rfid`, `write_rfid` и другие — часть полного интерфейса платформы iDryer. Они не обрабатываются `idryer-core` напрямую, а доставляются в продуктовый `CommandHandler`. Справочник: `contracts/portal_backend_status.md`.

## Форматы топиков

```c
// Формирование топика
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

Константы суффиксов определены в `mqtt/idryer_topics.h`:

```c
#define IDRYER_TOPIC_INFO               "info"
#define IDRYER_TOPIC_TELEMETRY          "telemetry"
#define IDRYER_TOPIC_STATUS             "status"
#define IDRYER_TOPIC_CONFIG             "config"
#define IDRYER_TOPIC_CONFIG_DELTA       "config/delta"
#define IDRYER_TOPIC_EVENTS             "events"
#define IDRYER_TOPIC_OFFLINE            "offline"
#define IDRYER_TOPIC_INTEGRATIONS_STATUS "integrations/status"
#define IDRYER_TOPIC_CMD_WILDCARD       "commands/#"
```
