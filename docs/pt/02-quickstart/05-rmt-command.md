Depois deste passo pressionar o botão Start no portal gerará um pulso RMT num pino de saída do ESP32. O exemplo segue iHeater Link, onde o pino aciona o iHeater STM32 via um optoacoplador.


O portal publica um comando `invoke` no tópico MQTT `idryer/{serial}/commands/invoke`. A biblioteca desserializa o JSON e chama o manipulador registado. O manipulador passa o comando para `RmtOutputAdapter`, que gera um quadro de pulso no pino seleccionado.

O manipulador é independente do pino específico ou protocolo — é uma simples função callback. RMT é uma implementação; PWM é outra, ver [06-pwm.md](06-pwm.md).


- ESP32-C3 ou ESP32 (RMT está disponível em todos os pinos GPIO)
- Uma carga no pino de saída (em iHeater Link — um STM32 via um optoacoplador)


**1. Declare RmtOutputAdapter** em `main.cpp`. Baseado em [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp):

```cpp

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

O pino de saída padrão é `IHEATER_TRIGGER_OUTPUT_PIN`. Defina-o via `build_flags`:

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. Inicialize** em `setup()`:

```cpp
s_output.begin();
```

`begin()` configura o canal RMT e inicia uma tarefa FreeRTOS de fundo que envia quadros de keepalive.

**3. Registe o manipulador de comando** em `setup()`:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action    = data["action"] | "";
    JsonObjectConst args  = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float    tempC  = args["tempC"]      | 0.0f;
        uint32_t durMin = args["durationMin"] | 0u;

        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = tempC;
        s_output.apply(cmd);

        device().status.mode[0]        = iDryer::UnitMode::Drying;
        device().status.targetTempC[0] = tempC;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::Off;
        cmd.targetTempC = 0.0f;
        s_output.apply(cmd);

        device().status.mode[0] = iDryer::UnitMode::Idle;
        device().publishStatusNow();
    }
});
```

**4. Em `loop()` — apenas chame `device().loop()`:**

```cpp
void loop() {
    device().loop();
}
```

Os quadros RMT são enviados a partir da tarefa FreeRTOS dentro de `s_output`, independentemente de `loop()`.


O portal publica no tópico MQTT `idryer/{serial}/commands/invoke`:

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

A biblioteca recebe esta mensagem e chama a callback registada com o `JsonObjectConst data` desserializado. O campo `action` determina o que fazer.

A lista de acções para cada tipo de dispositivo é definida em [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) sob `invoke_actions`.


Abra o portal → página do dispositivo → pressione o botão **Heat**. No Serial Monitor:

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

Os pulsos RMT aparecerão no pino de saída (verifique com um osciloscópio ou analisador lógico).


- [06-pwm.md](06-pwm.md) — substitua RMT com PWM (MOSFET, DC dimmer).
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — configuração RMT: frequência de pulso, código Off, intervalo de temperatura.
