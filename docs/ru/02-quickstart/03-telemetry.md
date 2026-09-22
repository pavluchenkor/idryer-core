# Телеметрия: датчик на карточке

После этой страницы устройство читает температуру и влажность с SHT31, а на карточке появляются их ячейки и своя величина — точка росы.

## Что понадобится

- модуль SHT31 (I2C, адрес 0x44 или 0x45);
- провода: SDA, SCL, 3,3 В, GND;
- в `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Подключайте датчик при отключённом питании платы.

## Код

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // ячейка температуры
    .hasAirHumidity  = true,    // ячейка влажности
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // адрес 0x44 или 0x45 — по перемычке модуля

// Точка росы по формуле Магнуса — пример своей величины.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // Нет данных — NAN: поле не уходит, на карточке «—».
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL — выводы вашей платы
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // опрос раз в 2 с
}

void loop() {
    s_link.loop();
}
```

## Как это работает

- `hasAirTemp` и `hasAirHumidity` в `Config` дают ячейки температуры и влажности на карточке. Код на портале не нужен.
- Поля `s_link.telemetry.*` ядро публикует само: раз в 30 с, пока хоть один юнит работает, иначе раз в 60 с. Периоды меняют поля `Config.telemetryPeriodMs` и `telemetryPeriodIdleMs`; ноль — значение из контракта.
- `NAN` — нет данных: поле не публикуется. Не подставляйте ноль, иначе на графике появится ложный провал.
- Своя величина: `onTelemetryPublish` добавляет поле в телеметрию перед публикацией, `card().sensor(id, label, unit, path, deviceClass)` объявляет его на карточке. `path` — путь в телеметрии, `deviceClass` необязателен: он задаёт иконку и формат.
- `s_link.every(ms, fn)` вызывает функцию из `loop()` с заданным периодом, не блокируя связь.

## Проверка

Через минуту после прошивки на карточке — температура, влажность и точка росы. Без датчика ячейки пустые, устройство работает дальше.

## Что дальше

[LED-лента](04-leds.md).
