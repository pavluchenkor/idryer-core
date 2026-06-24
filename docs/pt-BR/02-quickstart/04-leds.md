Depois deste passo uma fita WS2812B mudará de cor baseada na umidade e o brilho será controlável a partir do portal via comando `set`.


**Hardware:**

- Fita LED WS2812B (ou WS2811/SK6812)
- Resistor 330–470 Ω na linha de dados
- Fonte de alimentação 5 V (corrente depende do comprimento da fita; 300 LEDs consomem até 18 A)

**Software:**

- Biblioteca `fastled/FastLED @ ^3.6.0`

!!! warning
    Alimente a fita a partir de uma fonte de alimentação dedicada 5 V. Alimentar através do pino 3,3 V ou 5 V da placa é aceitável apenas para um teste rápido com alguns LEDs.


**1. Adicione FastLED** a `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... outras dependências
```

**2. Declare o buffer e executor** em `main.cpp`. Baseado em [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp


static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. Inicialize a fita** em `setup()`:

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

Substitua `60` com o número de LED atual de sua fita.

**4. Mude a cor por umidade** em `loop()`. Escala de cor: azul (seco) → amarelo → vermelho (úmido):

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airHumidityPct[0] = r.humidity;

        // Umidade 20 %–80 % → hue de 160 (azul) para 0 (vermelho).
        float h = constrain(r.humidity, 20.0f, 80.0f);
        uint8_t hue = (uint8_t)(160.0f - (h - 20.0f) / 60.0f * 160.0f);
        fill_solid(s_leds, s_executor.ledsCount(), CHSV(hue, 255, 200));
        FastLED.show();
    }
}
```

**5. Controle o brilho a partir do portal.** Registre um manipulador de comando `set` em `setup()`:

```cpp
s_link.onCommand("set", [](JsonObjectConst data) {
    int id  = data["id"]  | -1;
    int val = data["val"] | -1;
    if (id == MENU_BRIGHTNESS && val >= 0 && val <= 255) {
        FastLED.setBrightness((uint8_t)val);
        FastLED.show();
    }
});
```

`MENU_BRIGHTNESS` é uma constante de [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h), gerada a partir de `menu.yaml` via `regen.sh`. No seu próprio produto o nome e o valor diferirão — verifique `menu_ids.h` do seu projeto.


Após gravar, a fita deve acender-se na cor correspondente à umidade atual. Se nenhum sensor estiver presente, a fita permanece apagada (o executor não recebe dados).

Abra as configurações do dispositivo no portal e ajuste o controle deslizante de brilho — a fita responde imediatamente.


- [05-rmt-command.md](05-rmt-command.md) — acione um atuador a partir de um comando do portal (saída RMT).
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — API executor: zone pulse, animações, brilho.

---
