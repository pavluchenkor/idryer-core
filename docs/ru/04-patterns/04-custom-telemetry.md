# Кастомная телеметрия (product-specific payload)

## Когда использовать

Стандартный idryer-core публикует телеметрию из полей `Telemetry` — только то, что описано в общем контракте (`units[].temperature`, `humidity`, `heaterPower` и т.д.). Если ваш продукт должен добавить поля на верхний уровень JSON (например, `outputMode`, `targetTempC`, `active`) или включить данные, которых нет в `Telemetry`-структуре, — используйте этот рецепт.

Типичный случай: iHeater Link публикует `outputMode` и `targetTempC` поверх стандартных `units[]`, чтобы бэкенд мог передать фронту `heaterIntent` через WS-событие `telemetry:update`.

## Шаг 1 — Отключить авто-публикацию

В `Config` установите `telemetryPeriodMs = 0`. Это запрещает idryer-core самостоятельно публиковать урезанный payload:

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // публикуем вручную
    .statusPeriodMs    = 5000,
};
```

## Шаг 2 — Написать функцию публикации

Используйте `device().mqttClient()->publishTelemetry(doc)`. Включите все поля, которые ожидает бэкенд: и product-specific (верхний уровень), и стандартные `units[]`.

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Текущее намерение выхода (ваш аппаратный слой)
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Активная интеграция ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Product-specific поля верхнего уровня
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Стандартный блок units[] — бэкенд сохраняет историю
    // temperature/humidity = 0 если у устройства нет датчиков
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // добавит timestamp автоматически
}
```

## Шаг 3 — Вызывать из `loop()`

```cpp
void loop() {
    device().loop();

    static uint32_t s_lastTelMs = 0;
    if ((uint32_t)(millis() - s_lastTelMs) >= 5000u) {
        s_lastTelMs = millis();
        publishCustomTelemetry();
    }
    // ...
}
```

## Что нельзя делать

- **Не публикуйте одновременно** авто-телеметрию idryer-core (через ненулевой `telemetryPeriodMs`) и кастомную. Бэкенд получит два сообщения в один топик и обработает оба — данные задвоятся.
- **Не вызывайте `device().publishTelemetryNow()`** при `telemetryPeriodMs = 0` — она публикует стандартный урезанный payload без ваших product-specific полей.

## Почему библиотека не делает это сама

idryer-core уже публикует `heaterPower: 1` внутри `units[]` — этого формально достаточно, чтобы знать, что нагрев включён. Проблема не в библиотеке, а в бэкенде (`telemetry.handler.ts`): он смотрит именно на поле `outputMode` верхнего уровня и не выводит `heaterIntent` из стандартного `heaterPower`. Это техдолг на стороне бэкенда.

Текущий рецепт — временный обходной путь. Если бэкенд будет исправлен (станет выводить `heaterIntent` из `units[0].heaterPower`), можно вернуть `telemetryPeriodMs = 5000` и убрать `publishCustomTelemetry()` — стандартная библиотечная телеметрия будет работать без изменений.

Следите за изменениями в `telemetry.handler.ts`: как только там появится fallback на `heaterPower`, этот рецепт становится избыточным.
