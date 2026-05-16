# Adicionar um sensor

## Quando usar

Se o dispositivo precisa ler periodicamente um sensor físico (temperatura, humidade, peso, etc.) e publicar leituras para a nuvem ou um cliente LAN — use esta receita.

## Código pronto para usar

Copie para seu projeto e substitua `MyClimate` pelo nome de sua classe:

```cpp
// MyClimate.h — controlador de sensor de produto
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // não-bloqueante, sem delay()
    float temperature() const;
    float humidity()    const;
    bool  ok()          const;
};
```

```cpp
// main.cpp
#include <iDryer.h>
#include "MyClimate.h"

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link  s_link(CFG);
static MyClimate     s_climate;
static bool          s_sensorOk = false;

void setup() {
    s_sensorOk = s_climate.begin();
    s_link.begin();
}

void loop() {
    s_link.loop();

    if (s_sensorOk) {
        s_climate.tick(millis());
        if (s_climate.ok()) {
            s_link.telemetry.airTempC[0]       = s_climate.temperature();
            s_link.telemetry.airHumidityPct[0] = s_climate.humidity();
        }
    }
    // A publicação é automática, no temporizador telemetryPeriodMs de Config.
}
```

## Explicação

O produto apenas preenche os campos `s_link.telemetry.*` em `loop()`. A fachada os publica para MQTT e Local WS a cada `Config.telemetryPeriodMs` milissegundos por si só — não há necessidade de chamar `publishTelemetryNow()` manualmente. Esta é a diferença chave do MQTT manual: sem `StaticJsonDocument`, sem `publishTelemetry`, sem classe editora separada.

Se você precisar publicar leituras imediatamente fora do temporizador — chame `s_link.publishTelemetryNow()`.

Os sinalizadores `hasAirTemp` / `hasAirHumidity` em `Config` controlam quais campos aparecem no JSON. Um campo cujo sinalizador é `false` não é publicado.

Lista completa de campos de telemetria: [Campos de telemetria](../03-public-api/01-link-api-reference.md#telemetry-fields).

## Exemplo completo no repositório

Implementação de referência: `Sht31ClimateSensor` + preenchimento de `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]` em `iDryer-Storage/src/main.cpp`.
