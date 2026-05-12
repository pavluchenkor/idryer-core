# Шаг 03 — Телеметрия: публикуй данные датчика

После этого шага ESP32 будет читать температуру и влажность с датчика SHT31 и каждые 10 секунд публиковать их на портал. Портал отобразит данные на графике.

## Что понадобится

**Железо:**

- Датчик SHT31 на I2C-модуле (адрес 0x44 или 0x45)
- Провода: SDA, SCL, VCC (3.3 В), GND

**ПО:**

- PlatformIO
- Библиотека `robtillaart/SHT31 @ ^0.5.0`

## Шаги

**1. Подключить SHT31 к ESP32-C3** (пины по умолчанию Storage Link):

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3.3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    Подключайте датчик только при отключённом питании платы.

**2. Добавить библиотеку** в `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... остальные зависимости
```

**3. Включить Wire и датчик** в `main.cpp`. Пример из [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp
#include <Wire.h>
#include "storage/sensors/Sht31ClimateSensor.h"

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. Инициализировать** в `setup()`:

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // авто-определяет адрес 0x44 или 0x45
```

`begin()` возвращает `false`, если датчик не найден. Устройство продолжит работу без него.

**5. В `loop()` вызывать `tick()` и обновлять телеметрию:**

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airTempC[0]       = r.temperature;
        s_link.telemetry.airHumidityPct[0] = r.humidity;
    }
}
```

Библиотека публикует поля `telemetry.*` в MQTT автоматически с периодом `telemetryPeriodMs`, заданным в `iDryer::Config`. По умолчанию — 10 000 мс.

**6. В `iDryer::Config` включить capability:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```

## Проверка

Откройте Serial Monitor. При успешном обнаружении датчика:

```
[MAIN] SHT31 at 0x44
```

На портале перейдите на страницу устройства — показания температуры и влажности обновляются каждые 10 секунд.

Если датчик не найден, в логе будет предупреждение, устройство продолжит работу. Убедитесь, что адрес 0x44/0x45 не занят другим устройством на шине.

## Что дальше

- [04-leds.md](04-leds.md) — отображать влажность цветом LED-ленты.
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — реализация датчика.
