# Menü als Protokoll: menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## Drei Dateien — drei Rollen

| Datei | Besitzer | Beschreibt |
|------|-------|-----------|
| `src/menu/menu.yaml` | Ihr Produkt | Gerätemenü: Parameter, Aktionen, Struktur |
| `contracts/mqtt_contract.yaml` | idryer-core | Liste bekannter Bedeutungen: `canonical_roles` mit Beschriftungen in mehreren Sprachen |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | generiert | TypeScript-Typen für das Portal |

**`role:`** — ein semantischer Name für einen Menüpunkt. Die Firmware sagt „ich habe `iheater.heat_temp`“ statt „ich habe Punkt Nummer 35“. Interne Namen der Firmware können sich ändern, `role:` bleibt fest.

Das Menü spiegelt die Geräteeinstellungen. Die Firmware veröffentlicht jeden Punkt aus `menu.yaml`, mit oder ohne `role:`. Das Portal zeichnet jeden Punkt nach seinem Typ: Wert, Schalter, Aktion, Untermenü. `role:` gibt dem Punkt eine Beschriftung aus dem Vertrag in der Sprache des Nutzers; ohne `role:` zeigt das Portal den Namen, den das Gerät geschickt hat.

Die Gerätekarte baut nicht das Menü. Was die Karte zeigt und welche Operationen sie startet, beschreibt das Card-Manifest — siehe [Gerätekarte: das Card-Manifest](../09-add-product/02-add-widget.md). Eine Aktion der Karte kann Grenzen und Standardwert ihres Parameters aus einem Menüpunkt übernehmen.

---

## 1. Firmware-Build (`pio run`)

`menu.yaml` → `menu_gen.py` prüft jede `role:` gegen `canonical_roles` im Vertrag → ist eine Rolle unbekannt, bricht der Build mit einem Fehler und einer Liste gültiger Rollen ab → der Generator schreibt C++-Dateien nach `src/menu/`.

## 2. TypeScript für das Portal aktualisieren (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` erzeugt `mqtt-api.types.ts` und die Rollenbeschriftungen `roles.{lang}.json` → die Dateien werden ins Portal kopiert.

Ausführen, wenn sich der Vertrag ändert. Das Ergebnis committen.

## 3. Laufzeit: Gerät ↔ Portal

Die Firmware veröffentlicht das Menü im Topic `config` (das macht der Produktcode beim Befehl `get_config`; iDryer-Produkte auch, wenn sie online gehen) → das Portal-Backend speichert es → das Portal holt es mit `GET /devices/:id/menu-config` → jeder Punkt wird nach seinem Typ `t` (`val`, `tog`, `act`, `sub`) gezeichnet, die Beschriftung ist `canonical_roles[r].labels[lang]`, dann Englisch, dann der Name `n` vom Gerät.

Parameter (`min`, `max`, `val`) kommen aus dem Menüpunkt selbst — die Firmware kennt die aktuellen Werte.

Das Portal ändert einen Wert mit `commands/set { "id": <id>, "val": <value> }`.

---

## Eine Einstellung hinzufügen (NVS-Parameter)

```yaml
- id: my_param
  type: value
  role: my.param        # optional: eine Beschriftung aus dem Vertrag
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS-Schlüssel (≤ 15 Zeichen)
  persist: true
  scope: global
  default: 50
```

`bind` = NVS-Schlüssel. `persist: true` = der Wert übersteht einen Neustart.

`role:` ist kein freies Feld: der Wert muss aus `canonical_roles` im Vertrag stammen, sonst schlägt der Build fehl. Die Liste steht in `contracts/mqtt_contract.yaml` → `canonical_roles` oder in `menu.template.yaml`. Eine neue Rolle kommt zuerst in den Vertrag, danach `regen.sh`.

---

## Eine Operation zur Gerätekarte hinzufügen

Operationen (Start, Stopp, Heizen, Licht) werden als Aktionen der Karte deklariert, nicht als Menüpunkte. Ihre Grenzen können aus einem Menüpunkt kommen:

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

Die vollständige Beschreibung — [Gerätekarte: das Card-Manifest](../09-add-product/02-add-widget.md).

---

## Was man NICHT tun sollte

- Kein `widget:` in `menu.yaml` eintragen. Das Feld `widget` in `canonical_roles` ist Referenzinformation: Portal und App lesen es nicht.
- `mqtt-api.types.ts` nicht von Hand bearbeiten — es wird von `regen.sh` erzeugt.
- Die Flags `Config.hasXxx` nicht für neue Aktionen anfassen — sie sind nur für Telemetrie (Sensoren, Zustände).
