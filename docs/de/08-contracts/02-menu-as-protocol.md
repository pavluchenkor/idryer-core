# Menü als Protokoll: menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## Drei Dateien — drei Rollen

| Datei | Besitzer | Beschreibt |
|------|-------|-----------|
| `src/menu/menu.yaml` | Ihr Produkt | Geräte-Menü: Parameter, Aktionen, Struktur |
| `contracts/mqtt_contract.yaml` | idryer-core | Liste bekannter Bedeutungen: Was bedeutet jede `role:` und wie zeigt das Portal sie an |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | generiert | TypeScript-Typen für das Portal |

**`role:`** — Ein semantischer Name für ein Menü-Element. Die Firmware sagt "Ich habe `iheater.heat_start`", anstatt "Ich habe Knopf Nummer 35". Dies ist der stabile Vertrag zwischen Gerät und Portal — interne Firmware-Namen können sich ändern, `role:` bleibt fix.

**Widget** — Wie das Portal dieses Element anzeigt: Knopf, Schieber, Umschalter, oder eine komplexe Komponente (Farbwähler, Profileditor). Bestimmt durch den Vertrag über `role:`, nicht durch die Firmware.

Ein Menü-Element mit `role:` ist sichtbar für das Portal. Ohne `role:` — privat, nur auf der Geräteanzeige.

---

## 1. Firmware-Erstellung (`pio run`)

`menu.yaml` → `pre_gen_menu.py` validiert jede `role:` gegen `canonical_roles` im Vertrag → wenn eine Rolle unbekannt ist, schlägt der Build mit Fehler und einer Liste gültiger Rollen fehl → `menu_gen.py` generiert C++ Dateien in `src/menu/`

Validierung ist in den Build-Schritt eingebaut — Es ist physikalisch unmöglich, eine nicht existierende Rolle stumm zu verwenden.

## 2. TypeScript für das Portal aktualisieren (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` generiert `mqtt-api.types.ts` → Datei wird zu `frontend-v2/src/contracts/` kopiert

Manuell ausführen, wenn sich der Vertrag ändert. Das Ergebnis committen.

## 3. Laufzeit: Gerät ↔ Portal

Gerät verbindet → veröffentlicht Menü auf MQTT-Thema `config` → Portal liest jedes Element mit Feld `r:` → schlägt `CanonicalRoles[r].widget` auf → rendert Widget aus `WIDGET_REGISTRY`.

Parameter (`min`, `max`, `val`) kommen vom Menü-Element selbst — die Firmware kennt die aktuellen Werte.

---

## Wie man eine neue Aktion zum Portal-Dashboard hinzufügt

`role:` ist kein freies Feld. Der Wert muss aus der geschlossenen Liste in `canonical_roles` im Vertrag stammen. Sie können eine Rolle nicht ad hoc erfinden — der Build schlägt fehl. Siehe verfügbare Rollen in `contracts/mqtt_contract.yaml` → `canonical_roles` Sektion, oder in `menu.template.yaml`.

**1. Wählen Sie eine Rolle aus dem Vertrag.** Wenn keine passt — fügen Sie sie zuerst zu `mqtt_contract.yaml` → `canonical_roles` hinzu, dann führen Sie `regen.sh` aus:

```yaml
canonical_roles:
  my.action: { type: action, widget: button }
```

**2. Fügen Sie ein Element zu `menu.yaml` hinzu:**

```yaml
- id: my_action
  type: action
  role: my.action
  title: { ru: "МОЁ ДЕЙСТВИЕ", en: "MY ACTION" }
```

**3. Behandeln Sie es in Firmware (`main.cpp`):**

```cpp
if (action == "my.action") { /* do the thing */ }
```

`pio run` → Validierung → C++ → Firmware veröffentlicht `r: "my.action"` → Portal rendert Knopf.

---

## Wie man eine Einstellung (NVS-Parameter) hinzufügt

```yaml
- id: my_param
  type: value
  role: my.param        # only if it should appear on the portal; omit for display-only
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # NVS key (≤ 15 chars)
  persist: true
  scope: global
  default: 50
```

`bind` = NVS-Schlüssel. `persist: true` = Wert bleibt über Neustart.
Portal ändert den Wert über `commands/set { "id": <id>, "val": <value> }`.

---

## Was man NICHT tun sollte

- Fügen Sie nicht `widget:` zu `menu.yaml` hinzu — das Widget wird durch den Vertrag über `role:` bestimmt, nicht durch die Firmware
- Bearbeiten Sie nicht `mqtt-api.types.ts` von Hand — sie wird durch `regen.sh` generiert
- Ändern Sie nicht `Config.hasXxx` Flags für neue Aktionen — diese sind nur für Telemetrie (Sensoren, Status)
