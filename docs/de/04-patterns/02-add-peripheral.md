# Ein Peripheriegerät hinzufügen

## Wann man dies verwenden sollte

Wenn das Gerät Hardware auf einen Befehl aus der Cloud oder LAN steuern muss — Relais, Heizung, LED-Streifen, Motor — verwenden Sie dieses Rezept.

## Schlüsselfertig-Code

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
            s_link.publishStatusNow();  // reflect new state immediately
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
    // IMPORTANT: setCommandHandler — strictly AFTER begin().
    // begin() installs its own dispatcher; our handleCommand must overwrite it.
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## Erklärung

`s_link.runtime()->setCommandHandler(handleCommand)` ist der einzige Verbindungspunkt für den Command Handler. Nach diesem Aufruf erreichen alle eingehenden MQTT-Befehle (`invoke`, `set`, `drying`, `stop`, `ping`, `get_config` usw.) direkt `handleCommand`.

`s_link.publishStatusNow()` — rufen Sie nach jeder Änderung an `s_link.status.*` auf. Dies sendet den neuen Zustand sofort an das Portal und LAN-Clients, ohne auf den `statusPeriodMs` Timer zu warten.

Rufen Sie niemals `delay()` innerhalb von `handleCommand` auf — der Aufruf ist synchron vom MQTT-Callback; das Blockieren bricht die Sitzung. Platzieren Sie Timer im `loop()` des Produktobjekts.

### Alternative: `link.onRequest()`

Für Standard-Befehle (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) ist ein einfacherer Callback über `onRequest()` ausreichend — kein Parsing von rohem JSON nötig:

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
