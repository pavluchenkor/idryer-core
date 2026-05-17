# Veřejné API: iDryer::Link

`iDryer::Link` je jediný vstupní bod pro vývojáře zabudovaných systémů. Fasáda skrývá celý SDK stack: WiFi/Improv, cloud stavový stroj, HTTP claim, MQTT, lokální WebSocket, NVS. Produkt stačí vyplnit pole `telemetry`/`status`, zaregistrovat zpětná volání a zavolat `begin()`/`loop()`.

---

## Životní cyklus

Typický skelet `main.cpp`:

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // vyžadováno pouze při použití setCommandHandler

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — striktně PO begin(): begin() instaluje vlastní dispatcher
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## Konfigurace: `iDryer::Config`

Vyplňuje se jednou v `main.cpp`, předává se konstruktoru `Link`. Všechna pole používají agregovanou inicializaci (C++ designované inicializátory).

| Pole | Typ | Účel | Poznámka |
|------|-----|------|----------|
| `deviceType` | `DeviceType` | Typ zařízení | **povinné** |
| `unitsCount` | `uint8_t` | Počet jednotek (komor), 1..`MAX_UNITS` (4) | **povinné** |
| `hasAirTemp` | `bool` | Je přítomen senzor teploty vzduchu | false = pole je vynecháno z JSON |
| `hasAirHumidity` | `bool` | Je přítomen senzor vlhkosti | false = pole je vynecháno z JSON |
| `hasHeaterTemp` | `bool` | Je přítomen senzor teploty topidla | — |
| `hasHeaterPower` | `bool` | Je přítomen senzor výkonu topidla | — |
| `hasFanStatus` | `bool` | Je přítomen stav ventilátoru | — |
| `hasScales` | `bool` | Jsou přítomny váhy | — |
| `hasRfid` | `bool` | Je přítomen čtečka RFID | — |
| `allowHa` | `bool` | Povolovat integraci Home Assistant | false = SDK nevytváří klienta |
| `allowBambu` | `bool` | Povolovat integraci Bambu Lab LAN | — |
| `allowMoonraker` | `bool` | Povolovat integraci Moonraker/Klipper | — |
| `telemetryPeriodMs` | `uint32_t` | Období automatického publikování `Telemetry` (ms) | 0 = nepublikovat |
| `statusPeriodMs` | `uint32_t` | Období automatického publikování `Status` (ms) | 0 = nepublikovat |
| `hardwareVersion` | `const char*` | Řetězec verze hardwaru | **povinné** |
| `firmwareVersion` | `const char*` | Řetězec verze firmware | **povinné** |

---

## Třída `iDryer::Link`

### Konstruktor

```cpp
explicit Link(const Config& cfg);
```

Bere konfiguraci podle konstantní reference. `CFG` musí existovat po dobu celé životnosti objektu (typicky `static const`).

### Metody

#### `begin()`

```cpp
bool begin();
```

Zapne celý SDK stack: WiFi/Improv, cloud stavový stroj, HTTP claim, MQTT, lokální WebSocket, NVS persistence.

Zavolá se jednou v `setup()`. Vrací `true` při úspěšné inicializaci.

```cpp
void setup() {
    link.begin();
}
```

#### `loop()`

```cpp
void loop();
```

Jedina povinná metoda. Obsluhuje WiFi/MQTT/LocalAccess a automaticky publikuje telemetrii a stav na jejich časovačích.

Volá se při každé iteraci `loop()`. Bez tohoto volání není připojení udržováno.

```cpp
void loop() {
    link.loop();  // první v loop(), před logikou produktu
}
```

*Zdroj: `iDryer-Storage/src/main.cpp:253`, `iHeater-link/src/main.cpp:381`.*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

Okamžitě publikuje aktuální stav `link.telemetry`, bez ohledu na časovač `telemetryPeriodMs`.

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

Okamžitě publikuje aktuální stav `link.status`. Používá se po zpracování příkazu, když má být nový stav bez prodlevy odrážen na portálu.

```cpp
// iHeater-link/src/main.cpp:238
device().publishStatusNow();
```

#### `raiseEvent()`

```cpp
void raiseEvent(EventKind   severity,
                const char* event,
                const char* message,
                uint8_t     unitId = 0xFF);
```

Publikuje událost do tématu `idryer/{serial}/events`. Odesláno okamžitě.

| Parameter | Typ | Účel |
|-----------|-----|------|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | Kód události, např. `"OVERHEAT"`, `"SESSION_COMPLETE"` |
| `message` | `const char*` | Libovolný ladící text |
| `unitId` | `uint8_t` | Index jednotky (0..unitsCount-1) nebo `0xFF` pro zařízení |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

Registruje zpětné volání pro obchodní příkazy (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) přicházející přes MQTT nebo Local WS. Zdroj příkazu je transparentní.

`RequestCallback` = `std::function<void(const iDryer::Request&)>`

```cpp
link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start: myStart(r.unitId, r.targetTempC); break;
        case iDryer::RequestKind::Stop:  myStop(r.unitId);                 break;
        default: break;
    }
});
```

**Důležité:** pokud je nastaven `runtime()->setCommandHandler(...)`, toto zpětné volání není voláno — plný dispatcher zachycuje všechny příkazy.

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

Registruje zpětné volání pro `commands/profile` — víceúrovňový plán sušení.

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

Volá se, když se změní stav připojení integrace (HA, Bambu, Moonraker). Volitelné zpětné volání.

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

Volá se, když cloud claim flow vrátí PIN pro zadání na portálu.

`ClaimPinCallback` = `std::function<void(const char* pin, uint32_t expiresInSeconds)>`

```cpp
// iHeater-link/src/main.cpp:367
device().onClaimPin([](const char* pin, uint32_t expiresInSeconds) {
    Serial.printf("CLAIM_PIN:%s:%u\n", pin, expiresInSeconds);
});
```

#### `isOnline()`

```cpp
bool isOnline() const;
```

Vrací `true`, pokud je zařízení zaregistrováno a relace MQTT je aktivní.

```cpp
// iHeater-link/src/main.cpp:281
if (device().isOnline()) { ... }
```

#### `serial()`

```cpp
const char* serial() const;
```

Sériové číslo zařízení (řetězec z NVS, přiřazeno během claim). Prázdný řetězec před dokončením claim.

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

Zapíše přihlašovací údaje WiFi do NVS pouze pokud ještě nejsou nastaveny. Zavolá se před `begin()`. Používá se ve vývojových prostředích s pevnými přihlašovacími údaji.

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

Vždy přepíše přihlašovací údaje WiFi v NVS. Vývojářský pomocník a vynucené přeúčtování.

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

Ručně spustí cloud claim flow (provision → register → check-claim). Při úspěchu volá registrované zpětné volání `onClaimPin`. Vrací `true`, pokud byl požadavek přijat.

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

Odebere token zařízení z NVS a restartuje čip. Po restartu je zařízení neregistrované — automaticky se spustí flow autom. claim. Tato funkce se nevrací.

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

Výstup na správce integrací — pro zapojení na straně produktu (Moonraker chamber target callbacks, Bambu printer status, atd.).

Vyžaduje `#include <integrations/common/link_integrations_manager.h>`.

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

Výstup na SDK MQTT klienta — pro komponenty, které publikují vlastní témata nebo se integrují do směrování příkazů (např. `MenuBridge`).

Vyžaduje `#include <mqtt/mqtt_client.h>`.

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

Výstup na duální publikační pomocníka — odesílá jednu zátěž na MQTT i Local WS současně. Používá se pro odpovědi produktu, které se musí dostat na LAN klienta stejným způsobem jako automaticky publikovaná telemetrie.

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

Výstup na SDK runtime — používá se k nastavení plného obsluhy příkazů místo fasádního dispatcheru. Po `setCommandHandler(...)` nejsou fasádní `onRequest`/`onProfile` volány přes MQTT cestu.

**Důležité:** volej striktně po `begin()` — `begin()` instaluje vlastní dispatcher, který musí být přepsán.

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// Podpis obsluhy:
// void handleCommand(const char* cmd, JsonObjectConst data);
```

Vyžaduje `#include <runtime/idryer_runtime.h>`.

---

### Telemetrická pole {#telemetry-fields}

Vyplňuje produkt v `loop()`. SDK je čte na časovači `telemetryPeriodMs` a publikuje do MQTT a Local WS.

| Pole | Typ | Příznak konfigurace | Účel |
|------|-----|---------------------|------|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | Teplota vzduchu, °C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | Vlhkost, % |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | Teplota topidla, °C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | Výkon topidla, 0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | Stav ventilátoru |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | Váha, gramy |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

`unitId` = 0 pro první (nebo jedinou) jednotku. Index musí být < `Config.unitsCount`.

Pole `Status` — stejná struktura, ale pro operační stav:

| Pole | Typ | Účel |
|------|-----|------|
| `status.mode[unitId]` | `UnitMode` | Aktuální režim jednotky |
| `status.targetTempC[unitId]` | `float` | Cílová teplota |
| `status.durationS[unitId]` | `uint32_t` | Požadovaná doba trvání, s (0 = neurčitá) |
| `status.elapsedS[unitId]` | `uint32_t` | Čas uplynulý od začátku relace, s |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### Registrace zpětného volání přes runtime

Pokud je potřeba plná kontrola nad příchozími příkazy (např. produkt zpracovává `get_config`, `set`, nestandardní `invoke`):

```cpp
// Podpis — z idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// Registrace — striktně po link.begin()
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — řetězec příkazu (`"set"`, `"invoke"`, `"ping"`, `"get_config"`).
`data` — ArduinoJson `JsonObjectConst` s zátěží.

S tímto přístupem nejsou `onRequest()` a `onProfile()` volány z MQTT cesty — produkt zpracovává příkazy přímo.

---

## Výčty

### `iDryer::DeviceType`

| Hodnota | Číselně | Účel |
|---------|---------|------|
| `Unknown` | 0 | Žádný / nedefinovaný |
| `Dryer` | 1 | Sušička (iDryer LINK) |
| `Heater` | 2 | Topidlo |
| `StorageLink` | 4 | Storage Link (ESP32-C3 + LED) |
| `IHeaterLink` | 5 | iHeater Link |

### `iDryer::UnitMode`

`Idle`, `Drying`, `Storage`, `Profile`, `Fault`, `Unknown`

### `iDryer::EventKind`

`Info`, `Warning`, `Error`

### `iDryer::RequestKind`

`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`

### `iDryer::IntegrationKind`

`Ha`, `Bambu`, `Moonraker`

### `iDryer::IntegrationState`

`Disabled`, `Idle`, `Connecting`, `Online`, `ConfigMissing`, `Error`

---

## Kdy jít hlouběji

Fasáda je dostatečná pro většinu úkolů. Pokud potřebuješ pracovat pod úrovní fasády — s `idryer::IdryerRuntime`, `idryer::MqttClient`, `idryer::cloud::LinkIntegrationsManager` — podívej se do sekce Architektura.
