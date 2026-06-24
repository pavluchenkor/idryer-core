Depois deste passo o mesmo fluxo de comando do portal acionará uma saída PWM em vez de RMT. Um caso de uso típico é um aquecedor controlado via um MOSFET ou um regulador DC.


O executor é uma simples função callback. `RmtOutputAdapter` do passo anterior é uma implementação. Substitua-a com código `ledcWrite` — tudo o mais (MQTT, comandos, estado) permanece inalterado.


**1. Remova** o include `RmtOutputAdapter` e instância de `main.cpp`:

```cpp
// Remova:
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. Adicione inicialização PWM** em `setup()`:

```cpp

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // desligado no arranque
```

**3. No manipulador de comando** substitua `s_output.apply(cmd)` com `ledcWrite`:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action   = data["action"] | "";
    JsonObjectConst args = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float power01 = args["power"] | 1.0f;  // 0.0–1.0
        uint8_t duty  = (uint8_t)(power01 * 255.0f);
        ledcWrite(PWM_CHANNEL, duty);

        device().status.mode[0]             = iDryer::UnitMode::Drying;
        device().telemetry.heaterPower01[0] = power01;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        ledcWrite(PWM_CHANNEL, 0);

        device().status.mode[0]             = iDryer::UnitMode::Idle;
        device().telemetry.heaterPower01[0] = 0.0f;
        device().publishStatusNow();
    }
});
```

**4. `loop()` não muda:**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` é a API Arduino ESP32 para versões anteriores a 3.x. Na versão 3.x e posteriores use `ledcAttach(pin, freq, resolution)` e `ledcWrite(pin, duty)`. Verifique a sua versão em `platformio.ini` (`platform = espressif32@X.Y.Z`).


Pressione o botão **Heat** no portal. O pino de saída terá um sinal PWM com um duty cycle proporcional ao argumento `power`. Verifique com um multímetro (tensão média) ou um osciloscópio.


- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — referência completa da API `iDryer::Link`.
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — modelo para qualquer novo atuador.
