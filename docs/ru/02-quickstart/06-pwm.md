# Нагрев через ШИМ: режим, сессия, мощность

После этой страницы карточка запускает нагрев с температурой и временем, показывает сессию и мощность, а прибор держит температуру ШИМ-выходом по датчику.

## Что понадобится

- датчик SHT31 из [шага «Телеметрия»](03-telemetry.md);
- логический MOSFET (открывается от 3,3 В) на выводе 3 и нагреватель на его напряжение питания;
- в `platformio.ini` — `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    Нагреватель без термопредохранителя не оставляйте без присмотра: прошивка может зависнуть, а MOSFET — пробиться накоротко.

## Код

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // затвор MOSFET
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // заполнение 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // ячейка мощности нагрева
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "PWM Heater",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);

static void readSensor() {
    const bool ok = s_sht.read();
    s_link.telemetry.airTempC[0]       = ok ? s_sht.getTemperature() : NAN;
    s_link.telemetry.airHumidityPct[0] = ok ? s_sht.getHumidity()    : NAN;
}

static uint32_t s_startMs = 0;

static void stopHeat(uint8_t unit) {
    ledcWrite(PWM_CHANNEL, 0);
    s_link.telemetry.heaterPower01[unit] = 0.0f;
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 — без ограничения
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Раз в секунду: мощность по разнице с уставкой, время сессии.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // Нет показаний — нагреватель выключен: греть вслепую нельзя.
    float power = 0.0f;
    if (!isnan(t)) power = constrain((s_link.status.targetTempC[0] - t) * 0.1f, 0.0f, 1.0f);
    ledcWrite(PWM_CHANNEL, (uint32_t)(power * 255));
    s_link.telemetry.heaterPower01[0] = power;

    const uint32_t elapsed = (millis() - s_startMs) / 1000;
    s_link.status.elapsedS[0] = elapsed;
    if (s_link.status.durationS[0] && elapsed >= s_link.status.durationS[0]) stopHeat(0);
}

void setup() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(8, 9);
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");

    s_link.every(2000, readSensor);
    s_link.every(1000, regulate);
}

void loop() {
    s_link.loop();
}
```

## Как это работает

- Действие с режимом `HEATING` запускает сессию: колбэк выставляет режим, уставку, длительность и вызывает `publishStatusNow()`. Карточка по режиму показывает блок сессии и **Стоп**.
- `purpose` `target_temperature` и `duration` портал и приложение подписывают сами. Числа в `args` уже зажаты в пределы параметра.
- `durationS = 0` — без ограничения времени; `elapsedS` карточка показывает как прошедшее время.
- `hasHeater` даёт ячейку мощности. В работе ядро отдаёт среднее `heaterPower01` за два последних периода публикации, поэтому частые изменения ШИМ не прыгают на карточке.
- Регулятор здесь пропорциональный: 10 °C до уставки — полная мощность. Для точного удержания замените его на ПИД.
- `ledcSetup` / `ledcAttachPin` — API arduino-esp32 2.x. В 3.x — `ledcAttach(pin, freq, bits)` и `ledcWrite(pin, duty)`.

## Проверка

Запустите **Нагрев** на карточке: появится блок сессии с уставкой и временем, ячейка мощности покажет долю включения нагревателя. **Стоп** выключает выход.

## Что дальше

- [Карточка устройства: card-манифест](../09-add-product/02-add-widget.md) — параметры из меню прибора, профили, разметка карточки.
- [Как добавить новый продукт](../09-add-product/01-add-new-product.md).
