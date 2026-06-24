Po tomto kroku bude pás WS2812B měnit barvu na základě vlhkosti a jas bude ovladatelný z portálu přes příkaz `set`.


**Hardware:**

- LED pás WS2812B (nebo WS2811/SK6812)
- Odpor 330–470 Ω na datovém vodiči
- Napájecí zdroj 5 V (proud závisí na délce pásu; 300 LED pokreslí až 18 A)

**Software:**

- Knihovna `fastled/FastLED @ ^3.6.0`

!!! warning
    Napájejte pás z dedikovaného napájecího zdroje 5 V. Napájení přes pin 3,3 V nebo 5 V desky je přijatelné pouze pro rychlý test kouře s několika LED.


**1. Přidejte FastLED** do `platformio.ini`:

```ini
lib_deps =
    fastled/FastLED @ ^3.6.0
    ; ... další závislosti
```

**2. Deklarujte buffer a executor** v `main.cpp`. Založeno na [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp


static CRGB             s_leds[STORAGE_MAX_LEDS];
static LedStripExecutor s_executor(s_leds, STORAGE_MAX_LEDS);
```

**3. Inicializujte pás** v `setup()`:

```cpp
FastLED.addLeds<WS2812B, STORAGE_LED_PIN, GRB>(s_leds, 60);
FastLED.setBrightness(128);
FastLED.clear(true);
```

Nahraďte `60` skutečným počtem LED pro váš pás.

**4. Změňte barvu podle vlhkosti** v `loop()`. Barevná škála: modrá (suchá) → žlutá → červená (vlhká):

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airHumidityPct[0] = r.humidity;

        // Vlhkost 20 %–80 % → hue z 160 (modrá) na 0 (červená).
        float h = constrain(r.humidity, 20.0f, 80.0f);
        uint8_t hue = (uint8_t)(160.0f - (h - 20.0f) / 60.0f * 160.0f);
        fill_solid(s_leds, s_executor.ledsCount(), CHSV(hue, 255, 200));
        FastLED.show();
    }
}
```

**5. Ovládejte jas z portálu.** Zaregistrujte handler příkazu `set` v `setup()`:

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

`MENU_BRIGHTNESS` je konstanta z [`iDryer-Storage/src/menu/menu_ids.h`](../../../../iDryer-Storage/src/menu/menu_ids.h), vygenerovaná z `menu.yaml` přes `regen.sh`. Ve vašem produktu bude název a hodnota jiné — zkontrolujte `menu_ids.h` vašeho projektu.


Po naflashování by se pás měl rozsvítit v barvě odpovídající aktuální vlhkosti. Pokud žádný senzor není přítomen, pás zůstane vypnutý (executor neobdrží data).

Otevřete nastavení zařízení na portálu a posuňte posuvník jasu — pás reaguje okamžitě.


- [05-rmt-command.md](05-rmt-command.md) — ovládejte aktuátor z příkazu portálu (RMT výstup).
- [led_strip_executor.h](../../../../iDryer-Storage/src/storage/led_strip/led_strip_executor.h) — executor API: zone pulse, animace, jas.
