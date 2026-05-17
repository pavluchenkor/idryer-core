# Telemetria personalizada (carga específica do produto)

## Quando usar

A telemetria padrão do idryer-core publica apenas os campos definidos no contrato comum (`units[].temperature`, `humidity`, `heaterPower`, etc.). Se seu produto precisar adicionar campos de nível superior em JSON (p.ex. `outputMode`, `targetTempC`, `active`) ou incluir dados não presentes na estrutura `Telemetry`, use esta receita.

Um caso típico: iHeater Link publica `outputMode` e `targetTempC` junto com `units[]` padrão, para que o backend possa encaminhar `heaterIntent` para o frontend via evento `telemetry:update` WebSocket.

## Passo 1 — Desabilitar auto-publicação

Defina `telemetryPeriodMs = 0` em `Config`. Isso evita que idryer-core publique uma carga reduzida por conta própria:

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // publicar manualmente
    .statusPeriodMs    = 5000,
};
```

## Passo 2 — Escrever a função de publicação

Use `device().mqttClient()->publishTelemetry(doc)`. Inclua todos os campos que o backend espera: tanto específicos do produto (nível superior) quanto o bloco `units[]` padrão.

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Intenção atual de saída de hardware
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Integração ativa ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Campos de nível superior específicos do produto
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Bloco units[] padrão — backend armazena histórico deste
    // temperature/humidity = 0 se o dispositivo não tem sensores
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // timestamp é adicionado automaticamente
}
```

## Passo 3 — Chamar de `loop()`

```cpp
void loop() {
    device().loop();

    static uint32_t s_lastTelMs = 0;
    if ((uint32_t)(millis() - s_lastTelMs) >= 5000u) {
        s_lastTelMs = millis();
        publishCustomTelemetry();
    }
    // ...
}
```

## O que não fazer

- **Não publique ambos** a telemetria automática idryer-core (não-zero `telemetryPeriodMs`) e telemetria personalizada simultaneamente. O backend recebe duas mensagens no mesmo tópico e processa ambas — dados são duplicados.
- **Não chame `device().publishTelemetryNow()`** quando `telemetryPeriodMs = 0` — publica a carga reduzida padrão sem seus campos específicos do produto.

## Por que a biblioteca não faz isso

idryer-core já publica `heaterPower: 1` dentro de `units[]` — formalmente suficiente para saber que há aquecimento. O problema não está na biblioteca mas no backend (`telemetry.handler.ts`): procura especificamente um campo `outputMode` de nível superior e não deriva `heaterIntent` do `heaterPower` padrão. Esta é dívida técnica do lado do backend.

A receita atual é um workaround temporário. Se o backend for corrigido para derivar `heaterIntent` de `units[0].heaterPower`, você pode reverter para `telemetryPeriodMs = 5000` e remover `publishCustomTelemetry()` — a telemetria padrão da biblioteca funcionará sem mudanças.

Acompanhe as atualizações de `telemetry.handler.ts`: uma vez que um fallback em `heaterPower` seja adicionado lá, esta receita se torna redundante.
