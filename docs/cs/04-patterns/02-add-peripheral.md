# Přidání periférie

## Kdy použít

Pokud zařízení potřebuje ovládat hardware na příkaz z cloudu nebo LAN — relé, topidlo, LED pásku, motor — použij tento návod.

## Hotový kód

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
            s_link.publishStatusNow();  // okamžitě odrážej nový stav
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
    // DŮLEŽITÉ: setCommandHandler — striktně PO begin().
    // begin() instaluje vlastní dispatcher; náš handleCommand ho musí přepsat.
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## Vysvětlení

`s_link.runtime()->setCommandHandler(handleCommand)` je jediný spojovací bod pro obsluhu příkazů. Po tomto volání všechny příchozí MQTT příkazy (`invoke`, `set`, `drying`, `stop`, `ping`, `get_config`, atd.) dosáhnou přímo `handleCommand`.

`s_link.publishStatusNow()` — zavolej po každé změně `s_link.status.*`. Toto okamžitě odešle nový stav na portál a LAN klientům bez čekání na časovač `statusPeriodMs`.

Nikdy nevoláj `delay()` uvnitř `handleCommand` — volání je synchronní z MQTT zpětného volání; blokování jej zničí relaci. Umístěte časovače do `loop()` objektu produktu.

### Alternativa: `link.onRequest()`

Pro standardní příkazy (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) stačí jednodušší zpětné volání přes `onRequest()` — není třeba parsovat raw JSON:

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

`onRequest()` nepracuje vedle `setCommandHandler` — pokud je nastaven plný handler, zpětné volání `onRequest` se nevolá. Podrobnosti viz [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md).

## Plný příklad v repo

Referenční implementace: `handleCommand` zpracovávající `drying` / `stop` v `iHeater-link/src/main.cpp`.
