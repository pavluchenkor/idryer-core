# Публичный API: iDryer::Link

`iDryer::Link` — единственная точка входа для embedded-разработчика. Фасад скрывает весь SDK-стек: WiFi/Improv, cloud state machine, HTTP claim, MQTT, local WebSocket, NVS. Продукту достаточно заполнять поля `telemetry`/`status`, регистрировать callbacks и вызывать `begin()`/`loop()`.

---

## Жизненный цикл

Типичный скелет `main.cpp`:

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // только если нужен setCommandHandler

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — строго ПОСЛЕ begin(): begin() ставит свой диспетчер
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## Конфигурация: `iDryer::Config`

Заполняется один раз в `main.cpp`, передаётся в конструктор `Link`. Все поля — aggregate init (C++ designated initializers).

| Поле | Тип | Назначение | Примечание |
|------|-----|------------|------------|
| `deviceType` | `DeviceType` | Тип устройства | **обязательно** |
| `unitsCount` | `uint8_t` | Число юнитов (камер), 1..`MAX_UNITS` (4) | **обязательно** |
| `hasAirTemp` | `bool` | Есть датчик температуры воздуха | false = поле пропускается в JSON |
| `hasAirHumidity` | `bool` | Есть датчик влажности | false = поле пропускается в JSON |
| `hasHeaterTemp` | `bool` | Есть датчик температуры нагревателя | — |
| `hasHeaterPower` | `bool` | Есть датчик мощности нагревателя | — |
| `hasFanStatus` | `bool` | Есть статус вентилятора | — |
| `hasScales` | `bool` | Есть весы | — |
| `hasRfid` | `bool` | Есть RFID-считыватель | — |
| `allowHa` | `bool` | Разрешить интеграцию Home Assistant | false = SDK не создаёт клиента |
| `allowBambu` | `bool` | Разрешить интеграцию Bambu Lab LAN | — |
| `allowMoonraker` | `bool` | Разрешить интеграцию Moonraker/Klipper | — |
| `telemetryPeriodMs` | `uint32_t` | Период авто-публикации `Telemetry` (мс) | 0 = не публиковать |
| `statusPeriodMs` | `uint32_t` | Период авто-публикации `Status` (мс) | 0 = не публиковать |
| `hardwareVersion` | `const char*` | Версия железа (строка) | **обязательно** |
| `firmwareVersion` | `const char*` | Версия прошивки (строка) | **обязательно** |

---

## Класс `iDryer::Link`

### Конструктор

```cpp
explicit Link(const Config& cfg);
```

Принимает конфигурацию по константной ссылке. `CFG` должен существовать весь жизненный цикл объекта (обычно — `static const`).

### Методы

#### `begin()`

```cpp
bool begin();
```

Поднимает весь SDK-стек: WiFi/Improv, cloud state machine, HTTP claim, MQTT, local WebSocket, NVS persistence.

Вызывать один раз в `setup()`. Возвращает `true` при успешной инициализации.

```cpp
void setup() {
    link.begin();
}
```

#### `loop()`

```cpp
void loop();
```

Единственный обязательный тик. Обслуживает WiFi/MQTT/LocalAccess, авто-публикацию телеметрии и статуса по таймеру.

Вызывать каждую итерацию `loop()`. Без этого вызова соединение не поддерживается.

```cpp
void loop() {
    link.loop();  // первым в loop(), до продуктовой логики
}
```

*Источник: `iDryer-Storage/src/main.cpp:253`, `iHeater-link/src/main.cpp:381`.*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

Немедленно публикует текущее состояние `link.telemetry` вне зависимости от таймера `telemetryPeriodMs`.

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

Немедленно публикует текущее состояние `link.status`. Используется после обработки команды, когда нужно немедленно отразить новое состояние в портале.

```cpp
// iHeater-link/src/main.cpp:238
device().publishStatusNow();
```

#### `raiseEvent()`

```cpp
void raiseEvent(EventKind   severity,
                const char* event,
                const char* message,
                uint8_t     unitId = 0xFF);
```

Публикует событие в топик `idryer/{serial}/events`. Отправка немедленная.

| Параметр | Тип | Назначение |
|----------|-----|------------|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | Код события, например `"OVERHEAT"`, `"SESSION_COMPLETE"` |
| `message` | `const char*` | Произвольный текст для отладки |
| `unitId` | `uint8_t` | Номер юнита (0..unitsCount-1) или `0xFF` для device-wide |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

Регистрирует callback для бизнес-команд (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`), приходящих через MQTT или Local WS. Источник команды прозрачен.

`RequestCallback` = `std::function<void(const iDryer::Request&)>`

```cpp
link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start: myStart(r.unitId, r.targetTempC); break;
        case iDryer::RequestKind::Stop:  myStop(r.unitId);                 break;
        default: break;
    }
});
```

**Важно:** если установлен `runtime()->setCommandHandler(...)`, этот callback не вызывается — полный диспетчер перехватывает все команды.

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

Регистрирует callback для `commands/profile` — многоступенчатого расписания сушки.

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

Вызывается при изменении состояния подключения интеграции (HA, Bambu, Moonraker). Необязательный callback.

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

Вызывается, когда cloud claim flow возвращает PIN для ввода в портале.

`ClaimPinCallback` = `std::function<void(const char* pin, uint32_t expiresInSeconds)>`

```cpp
// iHeater-link/src/main.cpp:367
device().onClaimPin([](const char* pin, uint32_t expiresInSeconds) {
    Serial.printf("CLAIM_PIN:%s:%u\n", pin, expiresInSeconds);
});
```

#### `isOnline()`

```cpp
bool isOnline() const;
```

Возвращает `true`, если устройство зарегистрировано и MQTT-сессия активна.

```cpp
// iHeater-link/src/main.cpp:281
if (device().isOnline()) { ... }
```

#### `serial()`

```cpp
const char* serial() const;
```

Серийный номер устройства (строка из NVS, присваивается при claim). Пустая строка до завершения claim.

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

Записывает WiFi-credentials в NVS только если они ещё не установлены. Вызывать до `begin()`. Используется для dev-окружений с прошитыми учётными данными.

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

Всегда перезаписывает WiFi-credentials в NVS. Dev-helper и принудительный re-provisioning.

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

Вручную запускает cloud claim flow (provision → register → check-claim). При успехе вызывает зарегистрированный `onClaimPin` callback. Возвращает `true` если запрос принят.

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

Удаляет device token из NVS и перезагружает чип. После перезагрузки устройство не привязано — auto-claim flow запускается заново. Функция не возвращает управление.

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

Аутлет к менеджеру интеграций — для продуктовой стороны wiring (callbacks Moonraker chamber target, Bambu printer status и т.д.).

Требует `#include <integrations/common/link_integrations_manager.h>`.

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

Аутлет к MQTT-клиенту SDK — для компонентов, которые публикуют собственные топики или встраиваются в маршрутизацию команд (например, `MenuBridge`).

Требует `#include <mqtt/mqtt_client.h>`.

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

Аутлет к dual-publish хелперу — отправляет один payload одновременно в MQTT и Local WS. Использовать для продуктовых ответов, которые должны доходить до LAN-клиента так же, как авто-публикация телеметрии.

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

Аутлет к SDK runtime — используется для установки полного обработчика команд вместо фасадного диспетчера. После `setCommandHandler(...)` фасадные `onRequest`/`onProfile` не вызываются по MQTT-пути.

**Важно:** вызывать строго после `begin()` — `begin()` устанавливает свой диспетчер, который нужно перезаписать.

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// Сигнатура обработчика:
// void handleCommand(const char* cmd, JsonObjectConst data);
```

Требует `#include <runtime/idryer_runtime.h>`.

---

### Поля для записи телеметрии {#telemetry-fields}

Заполняются продуктом в `loop()`. SDK читает их по таймеру `telemetryPeriodMs` и публикует в MQTT и Local WS.

| Поле | Тип | Флаг в Config | Назначение |
|------|-----|---------------|------------|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | Температура воздуха, °C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | Влажность, % |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | Температура нагревателя, °C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | Мощность нагревателя, 0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | Статус вентилятора |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | Вес, граммы |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

`unitId` = 0 для первого (или единственного) юнита. Индекс должен быть < `Config.unitsCount`.

Поля `Status` — аналогично, но для операционного состояния:

| Поле | Тип | Назначение |
|------|-----|------------|
| `status.mode[unitId]` | `UnitMode` | Текущий режим юнита |
| `status.targetTempC[unitId]` | `float` | Целевая температура |
| `status.durationS[unitId]` | `uint32_t` | Запрошенная длительность, с (0 = бесконечно) |
| `status.elapsedS[unitId]` | `uint32_t` | Прошло с начала сессии, с |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### Callback-регистрация через runtime

Если нужен полный контроль над входящими командами (например, продукт обрабатывает `get_config`, `set`, нестандартные `invoke`):

```cpp
// Сигнатура — из idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// Регистрация — строго после link.begin()
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — строка команды (`"set"`, `"invoke"`, `"ping"`, `"get_config"`).
`data` — ArduinoJson `JsonObjectConst` с payload.

При таком подходе `onRequest()` и `onProfile()` не вызываются из MQTT-пути — продукт обрабатывает команды самостоятельно.

---

## Перечисления

### `iDryer::DeviceType`

| Значение | Числовое | Назначение |
|----------|----------|------------|
| `Unknown` | 0 | Нет / не определено |
| `Dryer` | 1 | Сушилка (iDryer LINK) |
| `Heater` | 2 | Нагреватель |
| `StorageLink` | 4 | Storage Link (ESP32-C3 + LED) |
| `IHeaterLink` | 5 | iHeater Link |

### `iDryer::UnitMode`

`Idle`, `Drying`, `Storage`, `Profile`, `Fault`, `Unknown`

### `iDryer::EventKind`

`Info`, `Warning`, `Error`

### `iDryer::RequestKind`

`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`

### `iDryer::IntegrationKind`

`Ha`, `Bambu`, `Moonraker`

### `iDryer::IntegrationState`

`Disabled`, `Idle`, `Connecting`, `Online`, `ConfigMissing`, `Error`

---

## Когда нужно идти глубже

Для большинства задач этого фасада достаточно. Если нужно работать ниже фасадного уровня — с `idryer::IdryerRuntime`, `idryer::MqttClient`, `idryer::cloud::LinkIntegrationsManager` — смотрите раздел Архитектура.
