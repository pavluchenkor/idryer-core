# Jak funguje idryer-core

idryer-core je knihovna pro ESP32. Převezme vše, co spojuje zařízení s portálem a aplikací:

- Wi-Fi: síť předá aplikace iDryer (ESPTouch) nebo webová stránka přes USB (Improv);
- spárování s účtem jednorázovým tokenem a odpojení;
- zabezpečenou relaci MQTT s opětovným připojením;
- přístup v místní síti: aplikace ovládá zařízení i bez internetu;
- publikaci telemetrie a stavu, doručování příkazů;
- aktualizaci firmwaru vzduchem;
- kartu zařízení na portálu a v aplikaci.

Vy píšete jen svou část: čtete senzory, ovládáte zátěž, určujete, co karta ukáže a jaké operace spouští.

## Jeden vstupní bod: `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // vlastní zařízení
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // buňka teploty
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // váš kód
}
```

| Vy | Jádro |
|---|---|
| vyplňujete `s_link.telemetry.*` | publikuje každých 30 s, v klidu každých 60 s |
| měníte `s_link.status.*` (režim, žádanou hodnotu, čas) a voláte `publishStatusNow()` | doručí stav; karta se přepne podle režimu |
| deklarujete `s_link.card()`: senzory, ovládací prvky, akce | sestaví card manifest a publikuje ho |
| registrujete `s_link.onCommand(...)` | předá příkazy z cloudu i z místní sítě |

## Karta zařízení

Portál a aplikace kreslí kartu podle card manifestu, který zařízení posílá:

- příznaky `Config.has*` dávají hotové buňky: teplotu, vlhkost, výkon topení, ventilátor a další;
- `card().sensor(...)` přidá vlastní veličinu podle její cesty v telemetrii;
- `card().action(...)` deklaruje operaci: režim jednotky po ní a parametry spuštění.

Kód na portálu k tomu není potřeba. Podrobně: [Karta zařízení: card manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml` je zdroj pravdy

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) popisuje protokol: topicy, pole telemetrie a stavu, schopnosti zařízení, card manifest. Generuje se z něj:

| Co | Kam |
|---|---|
| `iDryer::Config` (příznaky `has*`) a struktury API | `src/_generated/iDryer_api.h` |
| MQTT topicy | `contracts/_generated/mqtt_topics.h` |
| protokol UART ESP32 ↔ řadič | `contracts/_generated/uart_protocol.h` |
| typy TypeScript pro portál | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Soubory v `_generated/` neupravujte ručně: `contracts/regen.sh` je přepíše z kontraktu.

Vlastní veličina nevyžaduje změnu kontraktu: deklaruje ji `card().sensor(...)`. Kontrakt se mění, když novou schopnost potřebují všechny produkty: nejdřív `mqtt_contract.yaml`, pak `regen.sh`, pak kód.

## Produkty na jádře

- **iDryer Link**: komunikační modul sušičky, ESP32 vedle řadiče, výměna přes UART.
- **iDryer Storage**: osvětlení regálu s cívkami, adresovatelný pásek a senzor SHT31.
- **iHeater Link**: ovládání topení iHeater, integrace s Bambu Lab, Klipper/Moonraker a Home Assistant.

## Dál

[Spustit za 5 minut](01-five-minutes.md).
