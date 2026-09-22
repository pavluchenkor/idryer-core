# Действия без режима: реле и вызов прибора

После этой страницы на карточке появятся форма **Проветрить** с временем в минутах и кнопка вызова прибора в шапке.

## Что понадобится

- модуль реле с управлением от 3,3 В (или транзисторный ключ) на выводе 5;
- вентилятор или другая нагрузка на реле.

!!! warning
    Нагрузка от сети 230 В — только с изоляцией, предохранителем и в корпусе. Без опыта работы с сетевым напряжением собирайте на низковольтной нагрузке.

## Код

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // ячейка «вентилятор вкл/выкл»
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 — реле выключено
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // смена вкл/выкл публикуется сразу
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // раз в 250 мс
    if (s_ventUntilMs && (int32_t)(millis() - s_ventUntilMs) >= 0) {
        s_ventUntilMs = 0;
        digitalWrite(RELAY_PIN, LOW);
        s_link.telemetry.fanOn[0] = false;
    }
    if (s_blinks) digitalWrite(LED_PIN, --s_blinks % 2);
}

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // Без режима: юнит не переходит в «занят», форма остаётся на карточке.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Кнопка вызова прибора в шапке карточки.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## Как это работает

- Действие без режима (`mode` = `nullptr`) не переводит юнит в «занят»: карточка не показывает блок сессии, форма остаётся на месте.
- `purpose` `duration` — время в минутах: поле подписано **Время**, пределы — из `.param(...)`.
- `hasFan` даёт ячейку «вентилятор вкл/выкл». Смену `telemetry.fanOn` ядро публикует сразу, не дожидаясь периода.
- `.deviceClass("identify")` — постоянная кнопка вызова прибора в шапке карточки; `clear_errors` — сброс ошибок.

## Проверка

На карточке задайте время и нажмите **Проветрить**: реле включится, ячейка вентилятора покажет «вкл». Кнопка вызова в шапке мигает LED платы.

## Что дальше

[Нагрев через ШИМ](06-pwm.md).
