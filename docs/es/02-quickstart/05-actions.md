# Acciones sin modo: un relé y llamar al dispositivo

Después de esta página la tarjeta tiene el formulario **Ventilate** con un tiempo en minutos y, en la cabecera, un botón para llamar al dispositivo.

## Qué necesitas

- un módulo de relé controlado desde 3,3 V (o un interruptor con transistor) en el pin 5;
- un ventilador u otra carga en el relé.

!!! warning
    Una carga de red de 230 V exige aislamiento, fusible y caja. Sin experiencia con la tensión de red, monta con una carga de baja tensión.

## Código

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // celda «ventilador encendido/apagado»
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0: relé apagado
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // un cambio encendido/apagado se publica al instante
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // cada 250 ms
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
    // Sin modo: la unidad no pasa a «ocupada», el formulario sigue en la tarjeta.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Botón para llamar al dispositivo, en la cabecera de la tarjeta.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## Cómo funciona

- Una acción sin modo (`mode` = `nullptr`) no pone la unidad en «ocupada»: la tarjeta no muestra el bloque de sesión, el formulario sigue en su sitio.
- El purpose `duration` es un tiempo en minutos: el campo recibe la etiqueta **Tiempo**, los límites vienen de `.param(...)`.
- `hasFan` da la celda «ventilador encendido/apagado». El núcleo publica al instante un cambio de `telemetry.fanOn`, sin esperar al periodo.
- `.deviceClass("identify")` es el botón fijo para llamar al dispositivo en la cabecera de la tarjeta; `clear_errors` es el reinicio de errores.

## Comprobación

En la tarjeta fija un tiempo y toca **Ventilate**: el relé se activa y la celda del ventilador indica «encendido». El botón de llamada de la cabecera hace parpadear el LED de la placa.

## Siguiente

[Calefacción por PWM](06-pwm.md).
