---
title: "Gerätekarte: das Card-Manifest"
description: "Wie eine Firmware auf idryer-core ihre Karte mit s_link.card() beschreibt: Sensoren, Aktionen mit Startparametern, was ans Portal geht und was Portal und App zeichnen."
---

# Gerätekarte: das Card-Manifest

Portal und mobile App bauen die Karte jedes Geräts aus seinem **Card-Manifest** — einer Beschreibung, die die Firmware über sich selbst veröffentlicht: was angezeigt wird und was sich steuern lässt. Ein neuer Gerätetyp braucht keinen Code im Portal oder in der App.

Das Manifest gehört zur Fassade `iDryer::Link` (`s_link.card()`). Die Low-Level-Runtime (`IdryerRuntime`) veröffentlicht es nicht.

Menü und Karte sind verschiedene Dinge. Das Menü spiegelt die Geräteeinstellungen: ein im Portal geänderter Wert wird in den Gerätespeicher geschrieben. Die Karte zeigt Messwerte und startet Operationen: Startparameter gehen mit einem Befehl mit und werden nicht ins Menü geschrieben. Ein Startparameter kann Grenzen und Standardwert aus einem Menüpunkt übernehmen.

---

## So funktioniert es

```text
Firmware: Deklarationen s_link.card()
   │  der Core baut JSON und veröffentlicht es retained (QoS 1) nach idryer/{key}/card
   ▼
Portal-Backend: prüft das Manifest (Limits, erlaubte Typen und Felder), speichert es
   ▼
Portal und App: zeichnen die Karte
   │  der Nutzer drückt eine Schaltfläche
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
der Core leitet "card.<id>" an Ihren Callback weiter
```

Der Core veröffentlicht das Manifest nach dem Aufbau der MQTT-Verbindung und erneut, wenn sich die Deklaration oder ein Menüpunkt ändert, von dem sie abhängt.

---

## Entitäten: was angezeigt wird

Sensoren aus dem Vokabular des Ökosystems kommen über die `Config`-Flags dazu, Sie deklarieren sie nicht:

| `Config`-Flag | Zelle der Karte |
|---|---|
| `hasAirTemp` | Lufttemperatur |
| `hasAirHumidity` | Luftfeuchte |
| `hasHeaterTemp` | Heizertemperatur |
| `hasHeater` | Heizleistung |
| `hasFan` | Lüfter an / aus |
| `hasServo` | Klappe offen / zu |
| `hasWeight` | Wägemodule (Topic `weights`) |
| `hasRfid` | die Einheit hat einen RFID-Leser |

Eigener Wert: in die Telemetrie schreiben und einen Sensor mit JSON-Pfad deklarieren:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Einfache Bedienelemente sind ebenfalls Entitäten: `button`, `number`, `select`. Der Wert wird gesendet, sobald der Nutzer ihn ändert, der Core ruft Ihren Callback auf:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Aktionen: Operationen mit Startparametern

Eine Aktion ist eine Operation des Geräts: Trocknung starten, heizen, Licht einschalten, stoppen. Sie hat einen **Modus** — den Modus der Einheit nach der Aktion (`status.units[].mode`) — und **Parameter** mit einer Bedeutung (`purpose`). Was angezeigt wird, entscheidet die Karte nach dem aktuellen Modus der Einheit:

- der Modus der Einheit entspricht dem Modus einer Aktion → das Gerät ist damit beschäftigt: Sitzungsblock und Stopp-Schaltfläche (die Aktion mit Modus `IDLE`);
- sonst → das Startformular; mehrere Startaktionen → ein Modusumschalter.

Beispiel: ein beheizter Lagerschrank mit einem ESP32. Die Zieltemperatur ist der Menüpunkt `target_temp` (30–50 °C, Standard 45).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // bereits innerhalb 30..50
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
    menu_sync_state_to_cache();     // Menüwerte in den Cache, aus dem die Karte liest
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

Was der Callback erhält:

- Zahlen sind auf die Grenzen der Einheit begrenzt, eine fehlende Zahl wird durch den Standardwert ersetzt;
- `unit` — Index der Einheit aus `unitId` (`"U1"` → 0); ein Befehl für eine Einheit, die das Gerät nicht hat, wird ignoriert;
- nach dem Start `status.mode` setzen und `publishStatusNow()` aufrufen — nach dem Status wechselt die Karte zum Sitzungsblock.

### Aktions-API

| Aufruf | Was er tut |
|---|---|
| `card.action(id, mode, cb)` | eine Aktion; `mode` — Modus der Einheit danach, `nullptr` — der Modus ändert sich nicht |
| `.param(id, purpose, MENU_ID)` | eine Zahl: Grenzen, Schritt, Standardwert und Einheit aus dem Menüpunkt |
| `.param(id, purpose, min, max, step, def[, unit])` | eine Zahl mit eigenen Grenzen |
| `.ceiling(MENU_ID)` | die Obergrenze des vorherigen Parameters ist der Wert dieses Menüpunkts (z. B. maximale Lufttemperatur) |
| `.stages(id, "stages", MENU_ID)` | Profilstufen `[{temperature, ramp, hold}]`, Sekunden; Stufentemperatur innerhalb der Grenzen des Menüpunkts |
| `.select(id, purpose, options, count[, def])` | Auswahl aus einer Liste; ein Wert außerhalb der Liste wird durch den Standard ersetzt |
| `.color(id, purpose, "#FFFFFF")` | eine Farbe `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | feste Schaltflächen im Kartenkopf: Gerät finden, Fehler löschen |
| `.name("ru", "…").name("en", "…")` | Name der Aktion, wenn die Karte den Modus nicht kennt |

`purpose`-Werte: `target_temperature`, `target_humidity`, `duration` (Minuten, 0 — unbegrenzt), `stages`, `start_stage` (ab 0), `effect`, `rgb_color`. Die Karte nutzt sie für Feldbeschriftungen und Portal-Funktionen: das Trocknungs-Preset füllt `target_temperature` und `duration` einer Aktion mit Modus `DRYING`, ein Trocknungsprofil füllt `stages`.

Callbacks sind Funktionen oder Lambdas ohne Captures. Die Strings von `name` und die Auswahloptionen werden als Zeiger gespeichert — Literale oder statische Arrays verwenden.

---

## Was ans Portal geht

Der Schrank oben veröffentlicht (`Config`: `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`):

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

`limits` und `default` kommen aus dem Menüpunkt. Mit `.ceiling()` bekommt der Parameter zusätzlich `max_by` — den Titel des Menüpunkts, der die Obergrenze gekappt hat; die Karte nennt ihn, wenn ein Wert über der Grenze liegt.

---

## Was Portal und App zeichnen

Schema, kein Screenshot. Leerlauf:

```text
┌─ DIY Storage Cabinet ─────────── [Im Leerlauf] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                   │  ← Temperatur, Feuchte, Heizer
│ | 0 %     | aus             |                   │  ← Leistung, Lüfter (grau: 0 / aus)
│ [Temp. 45 °C          ]  [Lagerung]             │  ← Aktion "storage"
└─────────────────────────────────────────────────┘
```

Nach „Lagerung“ meldet das Gerät den Modus `STORAGE`, und dieselbe Karte zeigt den Sitzungsblock (Zieltemperatur und Dauer der Lagerung) und die Stopp-Schaltfläche — die Aktion mit Modus `IDLE`.

Regeln auf beiden Seiten:

- eine Zelle ist grau, wenn kein Wert da ist; die Leistung — auch bei 0; Lüfter und Klappe — im Zustand aus / zu;
- ein Wert außerhalb der Grenzen wird nicht ersetzt: der Start ist gesperrt, der Grund steht unter dem Formular;
- bei Firmware mit Aktionen im Manifest gibt es die Schaltflächen „Finden“ und „Fehler löschen“ nur, wenn sie Aktionen mit `identify` / `clear_errors` deklariert.

Wo die Karte erscheint:

| | Portal | App |
|---|---|---|
| ein Gerätetyp, der kein iDryer-Produkt ist | Dashboard-Karte aus dem Manifest, Aktionen darauf; Geräteseite — Messwerte und Aktionen (wenn das Manifest Aktionen hat) | Startseite — Messwerte; Aktionen — auf der Geräteseite |
| `deviceType = Dryer` | die Trocknerkarte mit denselben Aktionen aus dem Manifest | Startseite — Überwachung; Aktionen — auf der Geräteseite |

---

## Limits

| | Core | Portal-Backend |
|---|---|---|
| Entitäten | 16 deklarierte + automatische | 32 |
| Layout-Zeilen | 8 | 16 |
| ids pro Zeile | 4 | 4 |
| Aktionen | 8 | 8 |
| Parameter pro Aktion | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| Sprachen in `name` | `ru`, `en` | bis zu 4 |

`id`: lateinische Kleinbuchstaben, Ziffern und `_`, bis 24 Zeichen; Entitäten und Aktionen teilen einen Namensraum. Das Manifest-Dokument ist auf 4096 Byte begrenzt; passt es nicht, schreibt der Core `manifest overflows` ins Log und veröffentlicht es nicht.

Das Format selbst — `contracts/mqtt_contract.yaml`, Abschnitt `mqtt_only`, `suffix: card`.
