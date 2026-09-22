# Menu jako protokol: menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## Tři soubory — tři role

| Soubor | Vlastník | Popisuje |
|------|-------|-----------|
| `src/menu/menu.yaml` | váš produkt | menu zařízení: parametry, akce, struktura |
| `contracts/mqtt_contract.yaml` | idryer-core | seznam známých významů: `canonical_roles` s popisky v několika jazycích |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | generováno | typy TypeScript pro portál |

**`role:`** — sémantický název položky menu. Firmware říká „mám `iheater.heat_temp`“, ne „mám položku číslo 35“. Interní názvy firmwaru se mohou měnit, `role:` zůstává pevná.

Menu je zrcadlem nastavení zařízení. Firmware publikuje každou položku `menu.yaml`, s `role:` i bez ní. Portál kreslí každou položku podle jejího typu: hodnota, přepínač, akce, podmenu. `role:` dává položce popisek z kontraktu v jazyce uživatele; bez `role:` portál ukáže název, který poslalo zařízení.

Kartu zařízení menu nestaví. Co karta ukazuje a jaké operace spouští, popisuje card manifest — viz [Karta zařízení: card manifest](../09-add-product/02-add-widget.md). Akce karty může převzít meze a výchozí hodnotu svého parametru z položky menu.

---

## 1. Sestavení firmwaru (`pio run`)

`menu.yaml` → `menu_gen.py` ověří každou `role:` proti `canonical_roles` v kontraktu → pokud je role neznámá, sestavení skončí chybou a seznamem platných rolí → generátor zapíše soubory C++ do `src/menu/`.

## 2. Aktualizace TypeScriptu pro portál (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` vygeneruje `mqtt-api.types.ts` a popisky rolí `roles.{lang}.json` → soubory se zkopírují do portálu.

Spouštějte při změně kontraktu. Výsledek commitněte.

## 3. Běh: zařízení ↔ portál

Firmware publikuje menu do topicu `config` (dělá to kód produktu na příkaz `get_config`; produkty iDryer i při přechodu do online) → backend portálu ho uloží → portál ho získá přes `GET /devices/:id/menu-config` → každá položka se kreslí podle typu `t` (`val`, `tog`, `act`, `sub`), popisek je `canonical_roles[r].labels[lang]`, pak anglický, pak název `n` ze zařízení.

Parametry (`min`, `max`, `val`) přicházejí ze samotné položky menu — aktuální hodnoty zná firmware.

Portál mění hodnotu příkazem `commands/set { "id": <id>, "val": <value> }`.

---

## Jak přidat nastavení (parametr v NVS)

```yaml
- id: my_param
  type: value
  role: my.param        # volitelné: popisek z kontraktu
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

`bind` = klíč NVS. `persist: true` = hodnota přežije restart.

`role:` není volné pole: hodnota musí být z `canonical_roles` v kontraktu, jinak sestavení selže. Seznam je v `contracts/mqtt_contract.yaml` → `canonical_roles` nebo v `menu.template.yaml`. Nová role se nejdřív přidá do kontraktu, pak `regen.sh`.

---

## Jak přidat operaci na kartu zařízení

Operace (spuštění, zastavení, ohřev, osvětlení) se deklarují jako akce karty, ne jako položky menu. Jejich meze lze vzít z položky menu:

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

Úplný popis — [Karta zařízení: card manifest](../09-add-product/02-add-widget.md).

---

## Co NEdělat

- Nepřidávejte `widget:` do `menu.yaml`. Pole `widget` v `canonical_roles` je jen referenční: portál ani aplikace ho nečtou.
- Needitujte `mqtt-api.types.ts` ručně — generuje ho `regen.sh`.
- Nesahejte na příznaky `Config.hasXxx` kvůli novým akcím — jsou jen pro telemetrii (senzory, stavy).
