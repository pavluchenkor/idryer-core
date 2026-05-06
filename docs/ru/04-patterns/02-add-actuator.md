# Добавить actuator

## Когда использовать

Если устройству нужно управлять железом по команде из облака или LAN — реле, нагреватель, LED-лента, мотор — используйте этот рецепт.

## Готовый код

```cpp
// main.cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::StorageLink,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "1.0.0",
};

static iDryer::Link s_link(CFG);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (!cmd) return;

    if (strcmp(cmd, "invoke") == 0) {
        const char* action = data["action"] | "";

        if (strcmp(action, "fan.on") == 0) {
            myFan.on();
            s_link.publishStatusNow();  // немедленно отразить новое состояние
            return;
        }
        if (strcmp(action, "fan.off") == 0) {
            myFan.off();
            s_link.publishStatusNow();
            return;
        }
    }

    if (strcmp(cmd, "drying") == 0) {
        float targetTempC  = data["targetTempC"]  | 45.0f;
        uint32_t durationS = data["durationS"]    | 0;
        myHeater.start(targetTempC, durationS);
        s_link.status.mode[0]        = iDryer::UnitMode::Drying;
        s_link.status.targetTempC[0] = targetTempC;
        s_link.status.durationS[0]   = durationS;
        s_link.publishStatusNow();
        return;
    }

    if (strcmp(cmd, "stop") == 0) {
        myHeater.stop();
        s_link.status.mode[0] = iDryer::UnitMode::Idle;
        s_link.publishStatusNow();
        return;
    }
}

void setup() {
    myFan.begin();
    myHeater.begin();
    s_link.begin();
    // ВАЖНО: setCommandHandler — строго ПОСЛЕ begin().
    // begin() ставит свой диспетчер; наш handleCommand должен его перезаписать.
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## Объяснение

`s_link.runtime()->setCommandHandler(handleCommand)` — единственная точка подключения обработчика команд. После этого вызова все входящие MQTT-команды (`invoke`, `set`, `drying`, `stop`, `ping`, `get_config` и т.д.) поступают в `handleCommand` напрямую.

`s_link.publishStatusNow()` — вызывайте после каждого изменения `s_link.status.*`. Это немедленно отправляет новое состояние в портал и LAN-клиентам, не дожидаясь таймера `statusPeriodMs`.

Никогда не вызывайте `delay()` внутри `handleCommand` — вызов синхронный из MQTT-callback, блокировка рвёт сессию. Таймеры ставьте в `loop()` продуктового объекта.

### Альтернатива: `link.onRequest()`

Для типовых команд (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) достаточно более простого callback через `onRequest()` — без необходимости разбирать raw JSON:

```cpp
s_link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start:
            myHeater.start(r.targetTempC, r.durationS);
            break;
        case iDryer::RequestKind::Stop:
            myHeater.stop();
            break;
        default:
            break;
    }
});
```

`onRequest()` не работает совместно с `setCommandHandler` — если установлен полный обработчик, `onRequest`-callback не вызывается. Подробнее — в [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md).

## Полный пример в репо

Эталонная реализация — `handleCommand` с обработкой `drying` / `stop` в `iHeater-link/src/main.cpp`.
