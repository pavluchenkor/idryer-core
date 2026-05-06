# MqttClient

`MqttClient` — MQTT-клиент устройства. Оборачивает `PubSubClient`, управляет подключением и маршрутизацией входящих сообщений. Все топики формируются из серийного номера устройства автоматически.

## Инициализация

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

Вызывается `CloudStateMachine` после успешного provisioning. Не подключается сразу — устанавливает параметры и настраивает TLS.

Параметры:

- `serialNumber` — серийный номер устройства. Используется как MQTT client ID и username.
- `token` — токен устройства. Используется как MQTT password.

При сборке с флагом `MQTT_USE_TLS=1` клиент настраивает `WiFiClientSecure` с корневым CA Let's Encrypt (встроен в `root_ca.h`).

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // см. ниже «Размер буфера»
mqttClient_.setKeepAlive(60);
```

## Размер буфера {#buffer-size}

`PubSubClient` по умолчанию использует буфер 256 байт — этого хватает только на короткие сообщения. Для устройств iDryer этого мало: основной «тяжёлый» payload — это конфигурация устройства (меню), которая публикуется в топик `idryer/{serial}/config` целиком.

`MqttClient` устанавливает буфер в `MQTT_BUFFER_SIZE` и ограничивает размер чанка большого конфига `MQTT_CONFIG_CHUNK_SIZE`. Обе константы определены в `lib/idryer-core/src/mqtt/mqtt_client.h`:

```cpp
#define MQTT_BUFFER_SIZE        16384  // буфер PubSubClient
#define MQTT_CONFIG_CHUNK_SIZE  16000  // максимум данных в одном чанке config
```

Соотношение между ними:

- `MQTT_BUFFER_SIZE` (16384 байт) — верхняя граница **одного MQTT-сообщения**. Любой `publish*()` с payload больше этого размера будет отброшен `PubSubClient` без отправки.
- `MQTT_CONFIG_CHUNK_SIZE` (16000 байт) — максимальный объём `"d"` (полезной части) внутри одного чанка `publishConfigRaw`. Запас в 384 байта оставлен на envelope чанка: `{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}` плюс автоматическое поле `timestamp`.

### Откуда взяли 16384

Число подобрано не «по красоте», а от **максимального ожидаемого payload устройства**, который и есть передача settings/меню:

- Конфиг (меню) Storage Link и Link/iHeater сериализуется как JSON с экранированием. Полный snapshot текущего меню укладывается в ~10–14 КБ.
- Запас до 16384 покрывает рост числа пунктов меню без переезда на разбиение по чанкам.
- Значение кратно 4 КБ — удобно для аллокации на ESP32.

Если ваш продукт имеет более крупный конфиг (например, расширенное меню с большим числом пунктов или бинарными значениями), есть два пути:

1. **Поднять `MQTT_BUFFER_SIZE`** — переопределить через `build_flags` в `platformio.ini`:
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   Учтите расход RAM: `PubSubClient` держит этот буфер постоянно. На ESP32-C3 (~400 КБ свободного heap) 32 КБ — приемлемо, но дальше начинаются риски.

2. **Использовать `publishConfigRaw(json, length)`** — сам разобьёт payload на чанки по `MQTT_CONFIG_CHUNK_SIZE`, бэкенд соберёт по полям `tid` / `idx` / `total` / `last`. Этот путь предпочтителен для конфигов из RP2040, которые приходят по UART кусками и могут быть произвольной длины.

### Применимо к продуктовым публикациям

Те же 16384 байт — потолок для `publishTelemetry`, `publishStatus`, `publishEvent`. На практике телеметрия и события сильно меньше (сотни байт), и в этот лимит упираются только публикации конфига. Если в проекте есть периодическая публикация большого payload (например, дамп массива измерений) — оцените его размер заранее или разбивайте сами.

## Подключение

```cpp
bool MqttClient::connect();
```

Выполняет:

1. Подключение к брокеру с persistent session (`clean_session = false`). Persistent session обязательна — без неё команды, пришедшие пока устройство не в сети, теряются.
2. Устанавливает LWT-сообщение на топик `idryer/{serial}/offline` (QoS 1, не retained).
3. Подписывается на `idryer/{serial}/commands/#` (QoS 1). Делает до 3 попыток, при неудаче отключается.

Возвращает `true` если подключение и подписка успешны.

## Цикл

```cpp
void MqttClient::loop();
```

Вызывается каждую итерацию. Переподключается при обрыве, затем вызывает `PubSubClient::loop()` для получения входящих сообщений.

## Публикация

Все publish-методы добавляют поле `timestamp` (ISO 8601 UTC) если оно ещё не присутствует в документе.

| Метод | Топик | Retained |
|-------|-------|----------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | да |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | нет |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | да |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | нет |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | нет |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | да |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | нет |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | нет |

`publishConfigRaw` автоматически разбивает payload на чанки если размер превышает `MQTT_CONFIG_CHUNK_SIZE` (16000 байт). Каждый чанк содержит поля `tid`, `idx`, `total`, `last`, `d`.

!!! note
    `PubSubClient` публикует всегда на QoS 0, независимо от настроек топика. Это ограничение библиотеки.

## Приём команд

Входящие сообщения в топике `idryer/{serial}/commands/{cmd}` парсятся как JSON и передаются в зарегистрированный `CommandCallback`:

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

Часть `{cmd}` извлекается из топика и передаётся как первый аргумент. `IdryerRuntime` регистрирует этот колбэк в `begin()`.

## Вспомогательные методы

```cpp
static char* getIsoTimestamp(char* buffer); // буфер >= 32 байт
static char* generateUuid(char* buffer);    // буфер >= 37 байт
```

`generateUuid` генерирует UUID v4 на основе `esp_random()`.

## Ограничения

- Один экземпляр `MqttClient` на устройство (singleton через `instance_`).
- Максимальный размер одного JSON-сообщения — `MQTT_BUFFER_SIZE` (по умолчанию 16384 байт). Подбирается по размеру самого тяжёлого payload устройства — обычно это сериализованный конфиг (меню). Для бо́льших конфигов поднимите константу через `build_flags` или используйте `publishConfigRaw` с автоматическим разбиением на чанки. См. раздел [«Размер буфера»](#buffer-size).
- TLS включается флагом сборки `MQTT_USE_TLS`.
