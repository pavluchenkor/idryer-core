Po tomto kroku bude ESP32 číst teplotu a vlhkost ze senzoru SHT31 a publikovat hodnoty na portál každých 10 sekund. Portál je bude zobrazovat jako živý graf.


**Hardware:**

- SHT31 na I2C breakout modulu (adresa 0x44 nebo 0x45)
- Vodiče: SDA, SCL, VCC (3,3 V), GND

**Software:**

- PlatformIO
- Knihovna `robtillaart/SHT31 @ ^0.5.0`


**1. Připojte SHT31 k ESP32-C3** (výchozí piny používané Storage Link):

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3,3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    Připojujte senzor pouze s vypnutou deskou.

**2. Přidejte knihovnu** do `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... další závislosti
```

**3. Vložte Wire a senzor** do `main.cpp`. Založeno na [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. Inicializujte** v `setup()`:

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // auto-detects address 0x44 nebo 0x45
```

`begin()` vrací `false`, pokud není senzor nalezen. Zařízení bude pokračovat bez něj.

**5. Zavolejte `tick()` v `loop()` a aktualizujte telemetrická pole:**

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airTempC[0]       = r.temperature;
        s_link.telemetry.airHumidityPct[0] = r.humidity;
    }
}
```

Knihovna publikuje všechna pole `telemetry.*` na MQTT automaticky v intervalu nastaveném pomocí `telemetryPeriodMs` v `iDryer::Config`. Výchozí hodnota je 10 000 ms.

**6. Povolte schopnost v `iDryer::Config`:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```


Otevřete Serial Monitor. Při úspěšné detekci senzoru:

```
[MAIN] SHT31 at 0x44
```

Na portálu přejděte na stránku zařízení — údaje teploty a vlhkosti se aktualizují každých 10 sekund.

Pokud senzor není nalezen, je zaznamenáno varování a zařízení pokračuje v běhu. Zkontrolujte, že adresa 0x44/0x45 není obsazena jiným zařízením na sběrnici.


- [04-leds.md](04-leds.md) — vizualizujte vlhkost barvou LED pásu.
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — implementace senzoru.
