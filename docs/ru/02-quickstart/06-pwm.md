# Шаг 06 — Замена RMT на PWM

После этого шага тот же поток команд с портала будет управлять PWM-выходом вместо RMT. Типичное применение — нагреватель через MOSFET или диммер постоянного тока.

## Принцип

Executor — это обычная функция-обработчик. `RmtOutputAdapter` из предыдущего шага — одна из реализаций. Замените её на код с `ledcWrite` — всё остальное (MQTT, команды, статус) остаётся без изменений.

## Шаги

**1. Удалить** включение `RmtOutputAdapter` и его инстанс из `main.cpp`. Уберите:

```cpp
// Удалить:
#include "controller/RmtOutputAdapter.h"
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. Добавить PWM-инициализацию** в `setup()`:

```cpp
#define PWM_PIN     0      // GPIO для MOSFET
#define PWM_CHANNEL 0      // LEDC-канал (0–15)
#define PWM_FREQ_HZ 25000  // 25 кГц — тихо для большинства нагревателей
#define PWM_RES     8      // 8 бит → duty 0–255

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // выключено при старте
```

**3. В обработчике команды** заменить `s_output.apply(cmd)` на `ledcWrite`:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action   = data["action"] | "";
    JsonObjectConst args = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float power01 = args["power"] | 1.0f;  // 0.0–1.0
        uint8_t duty  = (uint8_t)(power01 * 255.0f);
        ledcWrite(PWM_CHANNEL, duty);

        device().status.mode[0]        = iDryer::UnitMode::Drying;
        device().telemetry.heaterPower01[0] = power01;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        ledcWrite(PWM_CHANNEL, 0);

        device().status.mode[0] = iDryer::UnitMode::Idle;
        device().telemetry.heaterPower01[0] = 0.0f;
        device().publishStatusNow();
    }
});
```

**4. `loop()` не меняется:**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` — Arduino ESP32 API (до версии arduino-esp32 3.x). В версии 3.x и выше используйте `ledcAttach(pin, freq, resolution)` и `ledcWrite(pin, duty)`. Проверьте версию в `platformio.ini` (`platform = espressif32@X.Y.Z`).

## Проверка

Нажмите кнопку **Heat** на портале. На выходном пине появится ШИМ-сигнал с заполнением, пропорциональным параметру `power`. Проверьте мультиметром (среднее напряжение) или осциллографом.

## Что дальше

- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — полный справочник API `iDryer::Link`.
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — шаблон для любого нового исполнительного механизма.
