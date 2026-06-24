Po tomto kroku stisknutí tlačítka Start na portálu vygeneruje RMT puls na výstupním pinu ESP32. Příklad následuje iHeater Link, kde pin ovládá iHeater STM32 přes optočlen.


Portál publikuje příkaz `invoke` na MQTT téma `idryer/{serial}/commands/invoke`. Knihovna deserializuje JSON a volá registrovaný handler. Handler předá příkaz `RmtOutputAdapter`, který vygeneruje pulsní rámec na vybraném pinu.

Handler je nezávislý na konkrétním pinu nebo protokolu — je to prostá callback funkce. RMT je jedna implementace; PWM je další, viz [06-pwm.md](06-pwm.md).


- ESP32-C3 nebo ESP32 (RMT je dostupný na všech GPIO pinech)
- Zátěž na výstupním pinu (v iHeater Link — STM32 přes optočlen)


**1. Deklarujte RmtOutputAdapter** v `main.cpp`. Založeno na [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp):

```cpp

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

Výchozí výstupní pin je `IHEATER_TRIGGER_OUTPUT_PIN`. Nastavte jej přes `build_flags`:

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. Inicializujte** v `setup()`:

```cpp
s_output.begin();
```

`begin()` konfiguruje RMT kanál a spustí úlohu FreeRTOS na pozadí, která odesílá udržovací rámce.

**3. Zaregistrujte handler příkazu** v `setup()`:

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

**4. V `loop()` — pouze zavolejte `device().loop()`:**

```cpp
void loop() {
    device().loop();
}
```

RMT rámce jsou odesílány z úlohy FreeRTOS uvnitř `s_output` nezávisle na `loop()`.


Portál publikuje na MQTT téma `idryer/{serial}/commands/invoke`:

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

Knihovna obdrží tuto zprávu a zavolá registrovaný callback s deserializovaným `JsonObjectConst data`. Pole `action` určuje, co dělat.

Seznam akcí pro každý typ zařízení je definován v [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) pod `invoke_actions`.


Otevřete portál → stránka zařízení → stiskněte tlačítko **Heat**. V Serial Monitoru:

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

RMT pulsy se objeví na výstupním pinu (ověřte osciloskopem nebo logickým analyzátorem).


- [06-pwm.md](06-pwm.md) — nahraďte RMT PWM (MOSFET, DC dimmer).
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — RMT konfigurace: frekvence pulsu, Off kód, teplotní rozsah.
