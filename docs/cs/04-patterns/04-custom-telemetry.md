# Vlastní telemetrie (zátěž specifická pro produkt)

## Kdy použít

Standardní telemetrie idryer-core publikuje pouze pole definovaná v běžné smlouvě (`units[].temperature`, `humidity`, `heaterPower`, atd.). Pokud tvůj produkt potřebuje přidat pole na nejvyšší úrovni JSON (např. `outputMode`, `targetTempC`, `active`) nebo zahrnout data, která nejsou v struktuře `Telemetry`, použij tento návod.

Typický případ: iHeater Link publikuje `outputMode` a `targetTempC` vedle standardního `units[]`, aby backend mohl přeposlat `heaterIntent` do frontendu přes `telemetry:update` WebSocket event.

## Krok 1 — Zakázání automatické publikace

Nastav `telemetryPeriodMs = 0` v `Config`. To zabrání idryer-core publikování zjednodušené zátěže samo:

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // publikuj ručně
    .statusPeriodMs    = 5000,
};
```

## Krok 2 — Napsání funkce publikace

Používej `device().mqttClient()->publishTelemetry(doc)`. Zahrň všechna pole, která backend očekává: jak specifickou pro produkt (nejvyšší úroveň), tak standardní blok `units[]`.

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Aktuální záměr výstupu hardwaru
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Aktivní integrace ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Pole na nejvyšší úrovni specifická pro produkt
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Standardní blok units[] — backend ukládá historii z tohoto
    // temperature/humidity = 0 pokud zařízení nemá senzory
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // časové razítko se přidá automaticky
}
```

## Krok 3 — Volání z `loop()`

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

## Co nedělat

- **Nepublikuj obojí** automatickou telemetrii idryer-core (nenulové `telemetryPeriodMs`) a vlastní telemetrii současně. Backend dostane dvě zprávy na stejném tématu a zpracuje obě — data se duplikují.
- **Nevoláj `device().publishTelemetryNow()`** když `telemetryPeriodMs = 0` — publikuje standardní zjednodušenou zátěž bez tvých polí specifických pro produkt.

## Proč to knihovna nedělá sama

idryer-core už publikuje `heaterPower: 1` uvnitř `units[]` — formálně dost aby bylo vidět, že se topí. Problém není v knihovně ale v backendu (`telemetry.handler.ts`): vypadá konkrétně po poli `outputMode` na nejvyšší úrovni a neodvozuje `heaterIntent` ze standardního `heaterPower`. To je technický dluh na straně backendu.

Aktuální návod je dočasné řešení. Pokud bude backend opraven aby odvodil `heaterIntent` z `units[0].heaterPower`, můžeš se vrátit k `telemetryPeriodMs = 5000` a odstranit `publishCustomTelemetry()` — standardní telemetrie knihovny bude fungovat bez jakýchkoli změn.

Sleduj aktualizace `telemetry.handler.ts`: jakmile bude tam přidáno fallback na `heaterPower`, tento návod bude nadbytečný.
