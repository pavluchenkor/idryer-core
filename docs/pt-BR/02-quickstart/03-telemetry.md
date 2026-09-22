# Telemetria: um sensor no card

Depois desta página, o dispositivo lê a temperatura e a umidade de um SHT31, e o card mostra as células delas e um valor seu, o ponto de orvalho.

## O que você precisa

- um módulo SHT31 (I2C, endereço 0x44 ou 0x45);
- fios: SDA, SCL, 3,3 V, GND;
- no `platformio.ini`:

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    Conecte o sensor com a placa desligada.

## Código

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // célula de temperatura
    .hasAirHumidity  = true,    // célula de umidade
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // endereço 0x44 ou 0x45, conforme o jumper do módulo

// Ponto de orvalho pela fórmula de Magnus: exemplo de um valor seu.
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // Sem dados = NAN: o campo não é enviado, o card mostra "—".
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA, SCL: pinos da sua placa
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // leitura a cada 2 s
}

void loop() {
    s_link.loop();
}
```

## Como funciona

- `hasAirTemp` e `hasAirHumidity` no `Config` dão células de temperatura e umidade no card. Não precisa de código no portal.
- O núcleo publica sozinho os campos `s_link.telemetry.*`: a cada 30 s enquanto pelo menos uma unidade trabalha, senão a cada 60 s. Os campos `Config.telemetryPeriodMs` e `telemetryPeriodIdleMs` mudam os períodos; zero significa o valor do contrato.
- `NAN` significa sem dados: o campo não é publicado. Não coloque zero no lugar, senão o gráfico mostra uma queda falsa.
- Um valor seu: `onTelemetryPublish` acrescenta um campo à telemetria antes da publicação, `card().sensor(id, label, unit, path, deviceClass)` declara ele no card. `path` é o caminho na telemetria; `deviceClass` é opcional e define o ícone e o formato.
- `s_link.every(ms, fn)` chama uma função a partir do `loop()` com o período dado, sem travar a conexão.

## Verificação

No minuto seguinte à gravação, o card mostra temperatura, umidade e ponto de orvalho. Sem sensor, as células ficam vazias e o dispositivo continua funcionando.

## Próximo passo

[Fita LED](04-leds.md).
