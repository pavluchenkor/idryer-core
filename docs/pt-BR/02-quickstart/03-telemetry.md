Depois deste passo o ESP32 lerá temperatura e umidade de um sensor SHT31 e publicará os valores no portal a cada 10 segundos. O portal os exibirá como um gráfico em tempo real.


**Hardware:**

- SHT31 num módulo de breakout I2C (endereço 0x44 ou 0x45)
- Fios: SDA, SCL, VCC (3,3 V), GND

**Software:**

- PlatformIO
- Biblioteca `robtillaart/SHT31 @ ^0.5.0`


**1. Conecte o SHT31 ao ESP32-C3** (pinos padrão utilizados pelo Storage Link):

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3,3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    Conecte o sensor apenas com a placa desligada.

**2. Adicione a biblioteca** a `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... outras dependências
```

**3. Inclua Wire e o sensor** em `main.cpp`. Baseado em [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp):

```cpp

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. Inicialize** em `setup()`:

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // detecta automaticamente endereço 0x44 ou 0x45
```

`begin()` retorna `false` se nenhum sensor for encontrado. O dispositivo continuará funcionando sem ele.

**5. Chame `tick()` em `loop()` e atualize os campos de telemetria:**

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

A biblioteca publica todos os campos `telemetry.*` em MQTT automaticamente no intervalo definido por `telemetryPeriodMs` em `iDryer::Config`. O padrão é 10 000 ms.

**6. Ative a capacidade em `iDryer::Config`:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```


Abra o Serial Monitor. Na detecção bem-sucedida do sensor:

```
[MAIN] SHT31 at 0x44
```

No portal, navegue até à página do dispositivo — as leituras de temperatura e umidade atualizam a cada 10 segundos.

Se o sensor não for encontrado, um aviso é registrado e o dispositivo continua funcionando. Verifique que o endereço 0x44/0x45 não está ocupado por outro dispositivo no barramento.


- [04-leds.md](04-leds.md) — visualize umidade com cor de fita LED.
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — implementação do sensor.

---
