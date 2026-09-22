---
title: "Karta zařízení: card manifest"
description: "Jak firmware na idryer-core popisuje svou kartu přes s_link.card(): senzory, akce s parametry spuštění, co odchází na portál a co kreslí portál a aplikace."
---

# Karta zařízení: card manifest

Portál i mobilní aplikace sestavují kartu každého zařízení z jeho **card manifestu** — popisu, který firmware publikuje o sobě: co zobrazit a co lze ovládat. Nový typ zařízení nepotřebuje žádný kód na portálu ani v aplikaci.

Manifest je součástí fasády `iDryer::Link` (`s_link.card()`). Nízkoúrovňový runtime (`IdryerRuntime`) ho nepublikuje.

Menu a karta jsou různé věci. Menu je zrcadlem nastavení zařízení: hodnota změněná z portálu se zapíše do paměti zařízení. Karta ukazuje měření a spouští operace: parametry spuštění odcházejí s jedním příkazem a do menu se nezapisují. Parametr spuštění může převzít meze a výchozí hodnotu z položky menu.

---

## Jak to funguje

```text
firmware: deklarace s_link.card()
   │  jádro sestaví JSON a publikuje ho retained (QoS 1) do idryer/{key}/card
   ▼
backend portálu: ověří manifest (limity, povolené typy a pole), uloží ho
   ▼
portál a aplikace: kreslí kartu
   │  uživatel stiskne tlačítko
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
jádro předá "card.<id>" vašemu callbacku
```

Jádro publikuje manifest po navázání spojení MQTT a znovu ho publikuje, když se změní deklarace nebo položka menu, na které závisí.

---

## Entity: co zobrazit

Senzory ze slovníku ekosystému se přidají podle příznaků `Config`, nedeklarujete je:

| Příznak `Config` | Buňka karty |
|---|---|
| `hasAirTemp` | teplota vzduchu |
| `hasAirHumidity` | vlhkost |
| `hasHeaterTemp` | teplota topení |
| `hasHeater` | výkon topení |
| `hasFan` | ventilátor zap / vyp |
| `hasServo` | klapka otevřená / zavřená |
| `hasWeight` | vážní moduly (topic `weights`) |
| `hasRfid` | jednotka má čtečku RFID |

Vlastní hodnota: přidejte ji do telemetrie a deklarujte senzor s cestou v JSON:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Jednoduché ovládací prvky jsou také entity: `button`, `number`, `select`. Hodnota se odešle hned, jak ji uživatel změní, jádro zavolá váš callback:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Akce: operace s parametry spuštění

Akce je operace zařízení: spustit sušení, ohřát, rozsvítit, zastavit. Má **režim** — režim jednotky po akci (`status.units[].mode`) — a **parametry** s významem (`purpose`). Co zobrazit, rozhoduje karta podle aktuálního režimu jednotky:

- režim jednotky odpovídá režimu akce → zařízení je jí zaměstnáno: blok relace a tlačítko Stop (akce s režimem `IDLE`);
- jinak → formulář spuštění; více akcí spuštění → přepínač režimů.

Příklad: vyhřívaná skříň pro skladování na jednom ESP32. Cílová teplota je položka menu `target_temp` (30–50 °C, výchozí 45).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // už v mezích 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // hodnoty menu do cache, ze které čte karta
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

Co dostane callback:

- čísla jsou omezena na meze jednotky, chybějící číslo nahradí výchozí hodnota;
- `unit` — index jednotky z `unitId` (`"U1"` → 0); příkaz pro jednotku, kterou zařízení nemá, se ignoruje;
- po spuštění nastavte `status.mode` a zavolejte `publishStatusNow()` — podle stavu karta přepne na blok relace.

### API akcí

| Volání | Co dělá |
|---|---|
| `card.action(id, mode, cb)` | akce; `mode` — režim jednotky po ní, `nullptr` — režim se nemění |
| `.param(id, purpose, MENU_ID)` | číslo: meze, krok, výchozí hodnota a jednotka z položky menu |
| `.param(id, purpose, min, max, step, def[, unit])` | číslo s vlastními mezemi |
| `.ceiling(MENU_ID)` | horní mez předchozího parametru je hodnota této položky menu (např. maximální teplota vzduchu) |
| `.stages(id, "stages", MENU_ID)` | fáze profilu `[{temperature, ramp, hold}]`, sekundy; teplota fáze v mezích položky menu |
| `.select(id, purpose, options, count[, def])` | výběr ze seznamu; hodnota mimo seznam se nahradí výchozí |
| `.color(id, purpose, "#FFFFFF")` | barva `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | stálá tlačítka v záhlaví: najít zařízení, smazat chyby |
| `.name("ru", "…").name("en", "…")` | název akce, když karta nezná režim |

Hodnoty `purpose`: `target_temperature`, `target_humidity`, `duration` (minuty, 0 — bez omezení), `stages`, `start_stage` (od 0), `effect`, `rgb_color`. Karta podle nich popisuje pole a zapojuje funkce portálu: předvolba sušení vyplní `target_temperature` a `duration` akce s režimem `DRYING`, profil sušení vyplní `stages`.

Callbacky jsou funkce nebo lambdy bez zachycení. Řetězce `name` a možnosti výběru se ukládají jako ukazatele — použijte literály nebo statická pole.

---

## Co odchází na portál

Skříň z příkladu publikuje (`Config`: `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`):

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` a `default` přišly z položky menu. S `.ceiling()` dostane parametr ještě `max_by` — název položky menu, která srazila horní mez; karta ho uvede, když je hodnota nad mezí.

---

## Co kreslí portál a aplikace

Schéma, ne snímek obrazovky. Nečinnost:

```text
┌─ DIY Storage Cabinet ────────────── [Nečinný] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                  │  ← teplota, vlhkost, topení
│ | 0 %     | vyp             |                  │  ← výkon, ventilátor (šedé: 0 / vyp)
│ [Tepl. 45 °C          ]  [Úložiště]            │  ← akce "storage"
└────────────────────────────────────────────────┘
```

Po spuštění zařízení hlásí režim `STORAGE` a stejná karta ukáže blok relace (cílová teplota a doba uložení) a tlačítko Stop — akci s režimem `IDLE`.

Pravidla na obou stranách:

- buňka je šedá, když chybí hodnota; výkon — také při 0; ventilátor a klapka — ve stavu vyp / zavřeno;
- hodnota mimo meze se nenahrazuje: spuštění je zablokováno a pod formulářem je důvod;
- u firmwaru s akcemi v manifestu jsou tlačítka najít a smazat chyby jen tehdy, když deklaruje akce s `identify` / `clear_errors`.

Kde se karta objeví:

| | Portál | Aplikace |
|---|---|---|
| typ zařízení, který není produktem iDryer | karta na dashboardu z manifestu, akce na ní; stránka zařízení — hodnoty a akce (pokud manifest akce má) | hlavní obrazovka — hodnoty; akce — na stránce zařízení |
| `deviceType = Dryer` | karta sušičky se stejnými akcemi z manifestu | hlavní obrazovka — sledování; akce — na stránce zařízení |

---

## Limity

| | Jádro | Backend portálu |
|---|---|---|
| entity | 16 deklarovaných + automatické | 32 |
| řádky layoutu | 8 | 16 |
| id na řádek | 4 | 4 |
| akce | 8 | 8 |
| parametry akce | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| jazyky `name` | `ru`, `en` | až 4 |

`id`: malá latinská písmena, číslice a `_`, až 24 znaků; entity a akce sdílejí jeden jmenný prostor. Dokument manifestu je omezen na 4096 bajtů; pokud se nevejde, jádro zapíše do logu `manifest overflows` a nepublikuje ho.

Samotný formát — `contracts/mqtt_contract.yaml`, sekce `mqtt_only`, `suffix: card`.
