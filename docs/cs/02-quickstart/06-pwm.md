Po tomto kroku bude stejný tok příkazu portálu ovládat PWM výstup místo RMT. Typický případ je ohřívač ovládaný přes MOSFET nebo DC dimmer.


Executor je prostá callback funkce. `RmtOutputAdapter` z předchozího kroku je jedna implementace. Nahraďte ji `ledcWrite` kódem — všechno ostatní (MQTT, příkazy, stav) zůstane beze změny.


**1. Odstraňte** include `RmtOutputAdapter` a instanci z `main.cpp`:

```cpp
// Odstraňte:
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. Přidejte inicializaci PWM** v `setup()`:

```cpp

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // vypnuto při startu
```

**3. V handleru příkazu** nahraďte `s_output.apply(cmd)` s `ledcWrite`:

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

**4. `loop()` se nemění:**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` je Arduino ESP32 API pro verze před 3.x. Ve verzi 3.x a novější použijte `ledcAttach(pin, freq, resolution)` a `ledcWrite(pin, duty)`. Zkontrolujte vaši verzi v `platformio.ini` (`platform = espressif32@X.Y.Z`).


Stiskněte tlačítko **Heat** na portálu. Výstupní pin bude nést PWM signál s duty cycle úměrným argumentu `power`. Ověřte multimetrem (průměrné napětí) nebo osciloskopem.


- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — plný reference API `iDryer::Link`.
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — šablona pro jakýkoliv nový aktuátor.
