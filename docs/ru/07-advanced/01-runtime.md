# IdryerRuntime

`IdryerRuntime` — верхний координатор устройства. Связывает `CloudStateMachine`, `ActionDispatcher`, `IProfile` и `MqttClient` в единую точку входа: `begin()` / `loop()`.

## Конструктор

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

Все четыре параметра обязательны. `profile` может быть `nullptr` (рантайм проверяет перед вызовом его методов).

## Запуск

```cpp
void begin();
```

Выполняет:

1. Регистрирует внутренний `CommandCallback` в `MqttClient`.
2. Вызывает `cloud->begin()`.

Вызывать один раз в `setup()`, после `setCommandHandler()`.

## Основной цикл

```cpp
void loop();
```

Каждый вызов:

1. Вызывает `cloud->loop()` — продвигает стейт-машину.
2. Вызывает `profile->loop()` — продуктовая логика.
3. При первом переходе в Online:
   - Вызывает `profile->onOnline()`.
   - Вызывает `profile->buildInfoJson()` и публикует результат в `idryer/{serial}/info` (retained).
4. При потере Online: сбрасывает флаг, чтобы следующий выход снова сработал.

## Встроенная обработка

### ping

```
commands/ping
```

Всегда обрабатывается рантаймом — не передаётся в `CommandHandler`.

Извлекает `data["timestamp"]` (формат `"YYYY-MM-DDTHH:MM:SSZ"`), синхронизирует системное время через `settimeofday()`, затем повторно публикует info-payload.

## CommandHandler — единый путь расширения

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

Все входящие команды, кроме `ping`, направляются в зарегистрированный `CommandHandler`.

Это **единственный официальный способ** расширить обработку команд. Используется для того, чтобы MQTT и локальный WS-транспорт сходились в одну точку:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(data["action"] | "", "device.getConfig") == 0))
    {
        // Ответить на оба транспорта:
        s_pub.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // product-specific commands...
}

// в setup():
runtime.setCommandHandler(handleCommand);   // MQTT
local.setCommandSink(handleCommand);        // локальный WS
```

!!! note "Если CommandHandler не зарегистрирован"
    Рантайм использует встроенный роутинг: `invoke` → `ActionDispatcher`, `set` → `ActionDispatcher`, `invoke device.getConfig` → публикация конфига. Это поведение по умолчанию — оставлено для совместимости.

## Статус Online

```cpp
bool isOnline() const;
```

Возвращает `true` если `CloudStateMachine` находится в состоянии `Online`.

## Что рантайм не делает

- Не публикует телеметрию — это задача продукта.
- Не управляет переподключением MQTT напрямую — это делает `CloudStateMachine`.
- Не знает о конкретных параметрах конфигурации устройства.
