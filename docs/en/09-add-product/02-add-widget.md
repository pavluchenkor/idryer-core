---
title: "Device card: the card manifest"
description: "How firmware on idryer-core describes its card with s_link.card(): sensors, actions with launch parameters, what goes to the portal and what the portal and the app draw."
---

# Device card: the card manifest

The portal and the mobile app build the card of any device from its **card manifest** — a description the firmware publishes about itself: what to show and what can be controlled. A new device type needs no code on the portal or in the app.

The manifest is part of the `iDryer::Link` facade (`s_link.card()`). The low-level runtime (`IdryerRuntime`) does not publish it.

The menu and the card are different things. The menu mirrors the device settings: a value changed from the portal is written to the device memory. The card shows measurements and runs operations: launch parameters go with one command and are not written to the menu. A launch parameter can take its limits and default from a menu item.

---

## How it works

```text
firmware: s_link.card() declarations
   │  the core builds JSON and publishes it retained (QoS 1) to idryer/{key}/card
   ▼
portal backend: validates the manifest (limits, allowed types and fields), stores it
   ▼
portal and app: draw the card
   │  the user presses a button
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
the core routes "card.<id>" to your callback
```

The core publishes the manifest after the MQTT connection is up and republishes it when the declaration or a menu item it depends on changes.

---

## Entities: what to show

Sensors from the ecosystem vocabulary are added from the `Config` flags, you do not declare them:

| `Config` flag | Card cell |
|---|---|
| `hasAirTemp` | air temperature |
| `hasAirHumidity` | humidity |
| `hasHeaterTemp` | heater temperature |
| `hasHeater` | heater power |
| `hasFan` | fan on / off |
| `hasServo` | damper open / closed |
| `hasWeight` | weight modules (topic `weights`) |
| `hasRfid` | the unit has an RFID reader |

Your own value: add it to telemetry and declare a sensor with its JSON path:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Simple controls are entities too: `button`, `number`, `select`. The value is sent as soon as the user changes it, the core calls your callback:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Actions: operations with launch parameters

An action is an operation of the device: start drying, heat, turn on the light, stop. It has a **mode** — the unit mode after the action (`status.units[].mode`) — and **parameters** with a meaning (`purpose`). The card decides what to show from the current unit mode:

- the mode of the unit equals the mode of an action → the device is busy with it: the session block and the Stop button (the action with mode `IDLE`);
- otherwise → the launch form; several launch actions → a mode switch.

Example: a heated storage cabinet on one ESP32. The target temperature is a menu item `target_temp` (30–50 °C, default 45).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // already within 30..50
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
    menu_sync_state_to_cache();     // menu values into the cache the card reads
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

What the callback receives:

- numbers are clamped to the unit limits, a missing number is replaced by the default;
- `unit` — the unit index from `unitId` (`"U1"` → 0); a command for a unit the device does not have is ignored;
- after a launch set `status.mode` and call `publishStatusNow()` — the card switches to the session block by the status.

### Action API

| Call | What it does |
|---|---|
| `card.action(id, mode, cb)` | an action; `mode` — the unit mode after it, `nullptr` — the mode does not change |
| `.param(id, purpose, MENU_ID)` | a number: limits, step, default and unit from the menu item |
| `.param(id, purpose, min, max, step, def[, unit])` | a number with its own limits |
| `.ceiling(MENU_ID)` | the upper limit of the previous parameter is the value of that menu item (for example, maximum air temperature) |
| `.stages(id, "stages", MENU_ID)` | profile stages `[{temperature, ramp, hold}]`, seconds; stage temperature within the menu item limits |
| `.select(id, purpose, options, count[, def])` | a choice from a list; a value outside the list is replaced by the default |
| `.color(id, purpose, "#FFFFFF")` | a color `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | permanent header buttons: locate the device, clear errors |
| `.name("ru", "…").name("en", "…")` | the action name, when the card does not know the mode |

`purpose` values: `target_temperature`, `target_humidity`, `duration` (minutes, 0 — unlimited), `stages`, `start_stage` (from 0), `effect`, `rgb_color`. The card uses them for field labels and for portal features: the drying preset fills `target_temperature` and `duration` of an action with mode `DRYING`, a drying profile fills `stages`.

Callbacks are functions or lambdas without captures. The strings of `name` and select options are stored as pointers — use literals or static arrays.

---

## What goes to the portal

The cabinet above publishes (`Config`: `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`):

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

`limits` and `default` came from the menu item. With `.ceiling()` the parameter also gets `max_by` — the title of the menu item that cut the upper limit; the card names it when a value is above the limit.

---

## What the portal and the app draw

Schematic, not a screenshot. Idle:

```text
┌─ DIY Storage Cabinet ─────────────── [Idle] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                │  ← temperature, humidity, heater
│ | 0 %     | off              |               │  ← power, fan (grey: 0 / off)
│ [Temp. 45 °C          ]  [Storage]           │  ← action "storage"
└──────────────────────────────────────────────┘
```

After "Storage" the device reports mode `STORAGE`, and the same card shows the session block (target temperature and time in storage) and the Stop button — the action with mode `IDLE`.

Rules on both sides:

- a cell is grey when there is no value; power — also at 0; fan and damper — when off / closed;
- a value outside the limits is not replaced: the launch is blocked and the reason is shown under the form;
- for firmware with actions in the manifest, the locate and clear-errors buttons exist only if it declares actions with `identify` / `clear_errors`.

Where the card appears:

| | Portal | App |
|---|---|---|
| a device type that is not an iDryer product | dashboard card from the manifest, actions on it; device page — readings and actions (if the manifest has actions) | home — readings; actions — on the device page |
| `deviceType = Dryer` | the dryer card with the same manifest actions | home — monitoring; actions — on the device page |

---

## Limits

| | Core | Portal backend |
|---|---|---|
| entities | 16 declared + automatic | 32 |
| layout rows | 8 | 16 |
| ids per row | 4 | 4 |
| actions | 8 | 8 |
| parameters per action | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| `name` languages | `ru`, `en` | up to 4 |

`id`: lowercase Latin letters, digits and `_`, up to 24 characters; entity and action ids share one namespace. The manifest document is limited to 4096 bytes; if it does not fit, the core logs `manifest overflows` and does not publish it.

The format itself — `contracts/mqtt_contract.yaml`, section `mqtt_only`, `suffix: card`.
