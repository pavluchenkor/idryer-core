# Ações sem modo: um relé e chamar o dispositivo

Depois desta página, o cartão tem o formulário **Ventilate** com um tempo em minutos e, no cabeçalho, um botão para chamar o dispositivo.

## O que é preciso

- um módulo de relé comandado a 3,3 V (ou um interruptor a transístor) no pino 5;
- uma ventoinha ou outra carga no relé.

!!! warning
    Uma carga na rede de 230 V exige isolamento, fusível e caixa. Sem experiência com a tensão da rede, monte com uma carga de baixa tensão.

## Código

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // célula «ventilador ligado/desligado»
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0: relé desligado
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // uma mudança ligado/desligado é publicada logo
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // a cada 250 ms
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
    // Sem modo: a unidade não fica «ocupada», o formulário fica no cartão.
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // Botão para chamar o dispositivo, no cabeçalho do cartão.
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## Como funciona

- Uma ação sem modo (`mode` = `nullptr`) não põe a unidade «ocupada»: o cartão não mostra o bloco da sessão, o formulário fica no lugar.
- O purpose `duration` é um tempo em minutos: o campo recebe a etiqueta **Tempo**, os limites vêm de `.param(...)`.
- `hasFan` dá a célula «ventilador ligado/desligado». O núcleo publica logo uma mudança de `telemetry.fanOn`, sem esperar pelo período.
- `.deviceClass("identify")` é o botão fixo para chamar o dispositivo no cabeçalho do cartão; `clear_errors` é a limpeza de erros.

## Verificação

No cartão defina um tempo e toque em **Ventilate**: o relé liga e a célula do ventilador indica «ligado». O botão de chamada do cabeçalho faz piscar o LED da placa.

## A seguir

[Aquecimento por PWM](06-pwm.md).
