# Telemetría: un sensor en la tarjeta

Después de esta página el dispositivo lee temperatura y humedad de un SHT31, y la tarjeta muestra sus celdas y un valor propio, el punto de rocío.

## Qué necesitas

- un módulo SHT31 (I2C, dirección 0x44 o 0x45);
- cables: SDA, SCL, 3,3 V, GND;
- en `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Conecta el sensor con la placa sin alimentación.

## Código

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // celda de temperatura
    .hasAirHumidity  = true,    // celda de humedad
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // dirección 0x44 o 0x45, según el puente del módulo

// Punto de rocío por la fórmula de Magnus: ejemplo de un valor propio.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // Sin datos = NAN: el campo no se envía, la tarjeta muestra «—».
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL: pines de tu placa
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // lectura cada 2 s
}

void loop() {
    s_link.loop();
}
```

## Cómo funciona

- `hasAirTemp` y `hasAirHumidity` en `Config` dan celdas de temperatura y humedad en la tarjeta. No hace falta código en el portal.
- El núcleo publica solo los campos `s_link.telemetry.*`: cada 30 s mientras al menos una unidad trabaja, si no cada 60 s. Los campos `Config.telemetryPeriodMs` y `telemetryPeriodIdleMs` cambian los periodos; cero significa el valor del contrato.
- `NAN` significa sin datos: el campo no se publica. No pongas cero en su lugar, o el gráfico mostrará una caída falsa.
- Un valor propio: `onTelemetryPublish` añade un campo a la telemetría antes de publicar, `card().sensor(id, label, unit, path, deviceClass)` lo declara en la tarjeta. `path` es la ruta en la telemetría; `deviceClass` es opcional y fija el icono y el formato.
- `s_link.every(ms, fn)` llama a una función desde `loop()` con el periodo dado, sin bloquear la conexión.

## Comprobación

En el minuto siguiente a la grabación, la tarjeta muestra temperatura, humedad y punto de rocío. Sin sensor las celdas quedan vacías y el dispositivo sigue funcionando.

## Siguiente

[Tira LED](04-leds.md).
