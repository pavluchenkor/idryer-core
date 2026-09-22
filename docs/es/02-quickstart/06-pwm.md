# Calefacción por PWM: modo, sesión, potencia

Después de esta página la tarjeta inicia la calefacción con una temperatura y un tiempo, muestra la sesión y la potencia, y el dispositivo mantiene la temperatura con una salida PWM según el sensor.

## Qué necesitas

- el sensor SHT31 del [paso de telemetría](03-telemetry.md);
- un MOSFET de nivel lógico (abre con 3,3 V) en el pin 3 y un calefactor para su tensión de alimentación;
- en `platformio.ini`: `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    No dejes sin vigilancia un calefactor sin fusible térmico: el firmware puede colgarse y el MOSFET puede fallar en cortocircuito.

## Código

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // puerta del MOSFET
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // ciclo de trabajo 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // celda de potencia de calefacción
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
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0: sin límite
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Una vez por segundo: potencia según la diferencia con la consigna, tiempo de la sesión.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // Sin lectura = calefactor apagado: nunca calentar a ciegas.
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

## Cómo funciona

- Una acción con el modo `HEATING` inicia una sesión: el callback fija el modo, la consigna y la duración y llama a `publishStatusNow()`. Según el modo, la tarjeta muestra el bloque de sesión y **Stop**.
- El portal y la aplicación rotulan solos los purposes `target_temperature` y `duration`. Los números en `args` ya están limitados a los límites del parámetro.
- `durationS = 0` significa sin límite de tiempo; la tarjeta muestra `elapsedS` como tiempo transcurrido.
- `hasHeater` da la celda de potencia. En funcionamiento, el núcleo envía la media de `heaterPower01` de los dos últimos periodos de publicación, así que los cambios frecuentes del PWM no saltan en la tarjeta.
- El regulador aquí es proporcional: 10 °C por debajo de la consigna es potencia completa. Para mantener con precisión, sustitúyelo por un PID.
- `ledcSetup` / `ledcAttachPin` son la API de arduino-esp32 2.x. En 3.x usa `ledcAttach(pin, freq, bits)` y `ledcWrite(pin, duty)`.

## Comprobación

Inicia **Heat** en la tarjeta: aparece el bloque de sesión con la consigna y el tiempo, y la celda de potencia muestra cuánto está encendido el calefactor. **Stop** apaga la salida.

## Siguiente

- [Tarjeta del dispositivo: el card manifest](../09-add-product/02-add-widget.md): parámetros desde el menú del dispositivo, perfiles, disposición de la tarjeta.
- [Cómo añadir un producto nuevo](../09-add-product/01-add-new-product.md).
