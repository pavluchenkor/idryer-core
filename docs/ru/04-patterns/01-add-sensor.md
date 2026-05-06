# Добавить sensor

## Когда использовать

Если устройству нужно периодически читать физический датчик (температура, влажность, вес и т.п.) и публиковать показания в облако или LAN-клиенту — используйте этот рецепт.

## Готовый код

Скопируйте в проект и замените `MyClimate` на имя своего класса:

```cpp
// MyClimate.h — продуктовый драйвер датчика
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // неблокирующий, без delay()
    float temperature() const;
    float humidity()    const;
    bool  ok()          const;
};
```

```cpp
// main.cpp
#include <iDryer.h>
#include "MyClimate.h"

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link  s_link(CFG);
static MyClimate     s_climate;
static bool          s_sensorOk = false;

void setup() {
    s_sensorOk = s_climate.begin();
    s_link.begin();
}

void loop() {
    s_link.loop();

    if (s_sensorOk) {
        s_climate.tick(millis());
        if (s_climate.ok()) {
            s_link.telemetry.airTempC[0]       = s_climate.temperature();
            s_link.telemetry.airHumidityPct[0] = s_climate.humidity();
        }
    }
    // Публикация — автоматическая, по telemetryPeriodMs из Config.
}
```

## Объяснение

Продукт только заполняет поля `s_link.telemetry.*` в `loop()`. Фасад сам публикует их в MQTT и Local WS каждые `Config.telemetryPeriodMs` миллисекунд — вручную вызывать `publishTelemetryNow()` не нужно. Это главное отличие от ручного MQTT: нет `StaticJsonDocument`, нет `publishTelemetry`, нет отдельного publisher-класса.

Если нужно немедленно опубликовать показания вне таймера — вызовите `s_link.publishTelemetryNow()`.

Флаги `hasAirTemp` / `hasAirHumidity` в `Config` управляют тем, какие поля попадают в JSON. Поле, флаг которого `false`, не публикуется.

Полный список полей телеметрии — в разделе [«Поля для записи телеметрии»](../03-public-api/01-link-api-reference.md#поля-для-записи-телеметрии).

## Полный пример в репо

Эталонная реализация — `Sht31ClimateSensor` + заполнение `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]` в `iDryer-Storage/src/main.cpp`.
