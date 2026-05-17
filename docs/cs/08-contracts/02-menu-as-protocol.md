# Menu jako protokol: menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## Tři soubory — tři role

| Soubor | Vlastník | Popisuje |
|------|-------|-----------|
| `src/menu/menu.yaml` | váš produkt | nabídku zařízení: parametry, akce, struktura |
| `contracts/mqtt_contract.yaml` | idryer-core | seznam známých významů: co znamená každá `role:` a jak portal zobrazuje |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | generováno | TypeScript typy pro portal |

**`role:`** — sémantické jméno pro položku nabídky. Firmware říká "mám `iheater.heat_start`" spíše než "mám tlačítko číslo 35". To je stabilní smlouva mezi zařízením a portálem — interní názvy firmwaru se mohou měnit, `role:` zůstává pevný.

**Widget** — jak portal zobrazuje tuto položku: tlačítko, posuvník, přepínač nebo složitou komponentu (výběr barvy, editor profilu). Určeno smlouvou prostřednictvím `role:`, nikoli firmwarem.

Položka nabídky s `role:` je viditelná portálu. Bez `role:` — soukromá, viditelná pouze na displeji zařízení.

---

## 1. Stavba firmwaru (`pio run`)

`menu.yaml` → `pre_gen_menu.py` ověří každou `role:` proti `canonical_roles` ve smlouvě → pokud je role neznámá, stavba selže s chybou a seznamem platných rolí → `menu_gen.py` vygeneruje soubory C++ do `src/menu/`

Ověřování je součástí kroku stavby — je fyzicky nemožné tiše použít neexistující roli.

## 2. Aktualizace TypeScriptu pro portál (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` vygeneruje `mqtt-api.types.ts` → soubor se zkopíruje do `frontend-v2/src/contracts/`

Spusťte ručně, když se změní smlouva. Potvrďte výsledek.

## 3. Runtime: zařízení ↔ portal

Zařízení se připojí → publikuje nabídku na téma MQTT `config` → portal přečte každou položku s polem `r:` → vyhledá `CanonicalRoles[r].widget` → vykreslí widget z `WIDGET_REGISTRY`.

Parametry (`min`, `max`, `val`) pocházejí z samotné položky nabídky — firmware zná aktuální hodnoty.

---

## Jak přidat novou akci na dashboard portálu

`role:` není volné pole. Hodnota musí pocházet ze uzavřeného seznamu v `canonical_roles` ve smlouvě. Nemůžete improvizovat roli za běhu — stavba selže. Dostupné role se podívejte na `contracts/mqtt_contract.yaml` → sekce `canonical_roles` nebo v `menu.template.yaml`.

**1. Vyberte roli ze smlouvy.** Pokud žádná nevyhovuje — nejdříve ji přidejte na `mqtt_contract.yaml` → `canonical_roles`, poté spusťte `regen.sh`:

```yaml
canonical_roles:
  my.action: { type: action, widget: button }
```

**2. Přidejte položku do `menu.yaml`:**

```yaml
- id: my_action
  type: action
  role: my.action
  title: { ru: "МОЁ ДЕЙСТВИЕ", en: "MY ACTION" }
```

**3. Zpracujte jej v firmwaru (`main.cpp`):**

```cpp
if (action == "my.action") { /* udělej věc */ }
```

`pio run` → ověření → C++ → firmware publikuje `r: "my.action"` → portal vykreslí tlačítko.

---

## Jak přidat nastavení (parametr NVS)

```yaml
- id: my_param
  type: value
  role: my.param        # pouze pokud by se měl zobrazit na portálu; vynechte pro displej
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # klíč NVS (≤ 15 znaků)
  persist: true
  scope: global
  default: 50
```

`bind` = klíč NVS. `persist: true` = hodnota přetrvá restart.
Portal změní hodnotu prostřednictvím `commands/set { "id": <id>, "val": <value> }`.

---

## Co NEDĚLAT

- Nepřidávejte `widget:` do `menu.yaml` — widget je určen smlouvou prostřednictvím `role:`, nikoli firmwarem
- Neupravujte `mqtt-api.types.ts` ručně — je generován pomocí `regen.sh`
- Nedotýkejte se příznaků `Config.hasXxx` pro nové akce — ty jsou pouze pro telemetrii (senzory, stavy)
