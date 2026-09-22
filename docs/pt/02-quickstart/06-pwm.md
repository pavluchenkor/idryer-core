# Aquecimento por PWM: modo, sessão, potência

Depois desta página, o cartão inicia o aquecimento com uma temperatura e um tempo, mostra a sessão e a potência, e o dispositivo mantém a temperatura com uma saída PWM de acordo com o sensor.

## O que é preciso

- o sensor SHT31 do [passo de telemetria](03-telemetry.md);
- um MOSFET de nível lógico (abre com 3,3 V) no pino 3 e um aquecedor para a sua tensão de alimentação;
- em `platformio.ini`: `robtillaart/SHT31 @ ^0.5.0`.

!!! warning
    Não deixe sem vigilância um aquecedor sem fusível térmico: o firmware pode bloquear e o MOSFET pode avariar em curto-circuito.

## Código

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // porta do MOSFET
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // ciclo de trabalho 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // célula de potência de aquecimento
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
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0: sem limite
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// Uma vez por segundo: potência pela diferença para o setpoint, tempo da sessão.
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // Sem leitura = aquecedor desligado: nunca aquecer às cegas.
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

## Como funciona

- Uma ação com o modo `HEATING` inicia uma sessão: o callback define o modo, o setpoint e a duração e chama `publishStatusNow()`. Pelo modo, o cartão mostra o bloco da sessão e **Stop**.
- O portal e a aplicação dão nome sozinhos aos purposes `target_temperature` e `duration`. Os números em `args` já estão limitados aos limites do parâmetro.
- `durationS = 0` significa sem limite de tempo; o cartão mostra `elapsedS` como tempo decorrido.
- `hasHeater` dá a célula de potência. Em funcionamento, o núcleo envia a média de `heaterPower01` dos dois últimos períodos de publicação, por isso as mudanças frequentes do PWM não saltam no cartão.
- O regulador aqui é proporcional: 10 °C abaixo do setpoint é potência máxima. Para manter com precisão, substitua-o por um PID.
- `ledcSetup` / `ledcAttachPin` são a API do arduino-esp32 2.x. No 3.x use `ledcAttach(pin, freq, bits)` e `ledcWrite(pin, duty)`.

## Verificação

Inicie **Heat** no cartão: aparece o bloco da sessão com o setpoint e o tempo, e a célula de potência mostra quanto o aquecedor está ligado. **Stop** desliga a saída.

## A seguir

- [Cartão do dispositivo: o card manifest](../09-add-product/02-add-widget.md): parâmetros do menu do dispositivo, perfis, disposição do cartão.
- [Como adicionar um produto novo](../09-add-product/01-add-new-product.md).
