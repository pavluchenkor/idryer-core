# Шаг 05 — Команды от портала: RMT-выход

После этого шага нажатие кнопки «Старт» на портале будет генерировать RMT-импульс на выходной пин ESP32. На примере iHeater Link: пин подключён к STM32 нагревателя.

## Принцип

Портал отправляет команду `invoke` через MQTT-топик `idryer/{serial}/commands/invoke`. Библиотека десериализует JSON и вызывает зарегистрированный обработчик. Обработчик передаёт команду в `RmtOutputAdapter`, который генерирует кадр импульсов на выбранный пин.

Команда не зависит от конкретного пина или протокола — это обычная функция-обработчик. RMT — одна из реализаций; другая — PWM, см. [06-pwm.md](06-pwm.md).

## Что понадобится

- ESP32-C3 или ESP32 (RMT доступен на всех пинах GPIO)
- Нагрузка на выходном пине (в примере iHeater Link — STM32 через оптопару)

## Шаги

**1. Объявить RmtOutputAdapter** в `main.cpp`. Основано на [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp):

```cpp
#include "controller/RmtOutputAdapter.h"

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

По умолчанию используется пин `IHEATER_TRIGGER_OUTPUT_PIN`. Задайте его через `build_flags`:

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. Инициализировать** в `setup()`:

```cpp
s_output.begin();
```

`begin()` настраивает RMT-канал и запускает фоновую FreeRTOS-задачу отправки keepalive-кадров.

**3. Зарегистрировать обработчик команды** в `setup()`:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action    = data["action"] | "";
    JsonObjectConst args  = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float    tempC  = args["tempC"]      | 0.0f;
        uint32_t durMin = args["durationMin"] | 0u;

        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode       = iheaterlink::ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = tempC;
        s_output.apply(cmd);

        device().status.mode[0]        = iDryer::UnitMode::Drying;
        device().status.targetTempC[0] = tempC;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::Off;
        cmd.targetTempC = 0.0f;
        s_output.apply(cmd);

        device().status.mode[0] = iDryer::UnitMode::Idle;
        device().publishStatusNow();
    }
});
```

**4. В `loop()` — только вызов `device().loop()`:**

```cpp
void loop() {
    device().loop();
}
```

RMT-кадры отправляются из FreeRTOS-задачи внутри `s_output` независимо от `loop()`.

## Как портал отправляет команду

Портал публикует в MQTT-топик `idryer/{serial}/commands/invoke`:

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

Библиотека получает это сообщение, вызывает зарегистрированный callback с десериализованным `JsonObjectConst data`. Поле `action` определяет, что делать.

Список actions для конкретного типа устройства задаётся в [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) в секции `invoke_actions`.

## Проверка

Откройте портал → страница устройства → кнопка **Heat**. В Serial Monitor:

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

На выходном пине RMT появятся импульсы (проверьте осциллографом или логическим анализатором).

## Что дальше

- [06-pwm.md](06-pwm.md) — заменить RMT на PWM (MOSFET, диммер).
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — конфигурация RMT: частота импульсов, код Off, диапазон температур.
