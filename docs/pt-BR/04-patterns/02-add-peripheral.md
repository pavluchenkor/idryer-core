# Adicionar uma periférica

## Quando usar

Se o dispositivo precisa controlar hardware em um comando da nuvem ou LAN — relé, aquecedor, fita LED, motor — use esta receita.

## Código pronto para usar

```cpp
// main.cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::StorageLink,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "1.0.0",
};

static iDryer::Link s_link(CFG);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (!cmd) return;

    if (strcmp(cmd, "invoke") == 0) {
        const char* action = data["action"] | "";

        if (strcmp(action, "fan.on") == 0) {
            myFan.on();
            s_link.publishStatusNow();  // refletir novo estado imediatamente
            return;
        }
        if (strcmp(action, "fan.off") == 0) {
            myFan.off();
            s_link.publishStatusNow();
            return;
        }
    }

    if (strcmp(cmd, "drying") == 0) {
        float targetTempC  = data["targetTempC"]  | 45.0f;
        uint32_t durationS = data["durationS"]    | 0;
        myHeater.start(targetTempC, durationS);
        s_link.status.mode[0]        = iDryer::UnitMode::Drying;
        s_link.status.targetTempC[0] = targetTempC;
        s_link.status.durationS[0]   = durationS;
        s_link.publishStatusNow();
        return;
    }

    if (strcmp(cmd, "stop") == 0) {
        myHeater.stop();
        s_link.status.mode[0] = iDryer::UnitMode::Idle;
        s_link.publishStatusNow();
        return;
    }
}

void setup() {
    myFan.begin();
    myHeater.begin();
    s_link.begin();
    // IMPORTANTE: setCommandHandler — estritamente APÓS begin().
    // begin() instala seu próprio dispatcher; nosso handleCommand deve sobrescrevê-lo.
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## Explicação

`s_link.runtime()->setCommandHandler(handleCommand)` é o único ponto de conexão para o manipulador de comandos. Após essa chamada, todos os comandos MQTT recebidos (`invoke`, `set`, `drying`, `stop`, `ping`, `get_config`, etc.) chegam diretamente a `handleCommand`.

`s_link.publishStatusNow()` — chame após cada mudança em `s_link.status.*`. Isso envia imediatamente o novo estado para o portal e clientes LAN sem esperar pelo temporizador `statusPeriodMs`.

Nunca chame `delay()` dentro de `handleCommand` — a chamada é síncrona de um callback MQTT; bloqueá-lo quebra a sessão. Coloque temporizadores no `loop()` do objeto do produto.

### Alternativa: `link.onRequest()`

Para comandos padrão (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) um callback mais simples via `onRequest()` é suficiente — não há necessidade de analisar JSON bruto:

```cpp
s_link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start:
            myHeater.start(r.targetTempC, r.durationS);
            break;
        case iDryer::RequestKind::Stop:
            myHeater.stop();
            break;
        default:
            break;
    }
});
```

`onRequest()` não funciona junto com `setCommandHandler` — se o manipulador completo for definido, o callback `onRequest` não será chamado. Consulte [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) para detalhes.

## Exemplo completo no repositório

Implementação de referência: `handleCommand` tratando `drying` / `stop` em `iHeater-link/src/main.cpp`.
