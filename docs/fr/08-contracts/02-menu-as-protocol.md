# Le menu comme protocole : menu.yaml ↔ mqtt_contract.yaml ↔ portail

---

## Trois fichiers, trois rôles

| Fichier | Propriétaire | Décrit |
|------|-------|-----------|
| `src/menu/menu.yaml` | votre produit | menu de l'appareil : paramètres, actions, structure |
| `contracts/mqtt_contract.yaml` | idryer-core | liste des sens connus : `canonical_roles` avec des libellés en plusieurs langues |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | généré | types TypeScript pour le portail |

**`role:`** — nom sémantique d'un élément du menu. Le firmware dit « j'ai `iheater.heat_temp` » et non « j'ai l'élément numéro 35 ». Les noms internes du firmware peuvent changer, `role:` reste fixe.

Le menu reflète les réglages de l'appareil. Le firmware publie tous les éléments de `menu.yaml`, avec ou sans `role:`. Le portail dessine chaque élément selon son type : valeur, interrupteur, action, sous-menu. `role:` donne à l'élément un libellé du contrat dans la langue de l'utilisateur ; sans `role:`, le portail affiche le nom envoyé par l'appareil.

Le menu ne construit pas la carte de l'appareil. Ce que montre la carte et les opérations qu'elle lance sont décrits par le card manifest — voir [Carte de l'appareil : le card manifest](../09-add-product/02-add-widget.md). Une action de la carte peut prendre les limites et la valeur par défaut de son paramètre dans un élément du menu.

---

## 1. Compilation du firmware (`pio run`)

`menu.yaml` → `menu_gen.py` vérifie chaque `role:` dans `canonical_roles` du contrat → si un rôle est inconnu, la compilation échoue avec une erreur et la liste des rôles valides → le générateur écrit les fichiers C++ dans `src/menu/`.

## 2. Mise à jour du TypeScript pour le portail (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` génère `mqtt-api.types.ts` et les libellés des rôles `roles.{lang}.json` → les fichiers sont copiés dans le portail.

À lancer quand le contrat change. Committez le résultat.

## 3. À l'exécution : appareil ↔ portail

Le firmware publie le menu sur le topic `config` (c'est le code du produit qui le fait sur la commande `get_config` ; les produits iDryer aussi à leur mise en ligne) → le backend du portail l'enregistre → le portail l'obtient avec `GET /devices/:id/menu-config` → chaque élément est dessiné selon son type `t` (`val`, `tog`, `act`, `sub`) ; le libellé est `canonical_roles[r].labels[lang]`, puis l'anglais, puis le nom `n` envoyé par l'appareil.

Les paramètres (`min`, `max`, `val`) viennent de l'élément du menu lui-même : le firmware connaît les valeurs actuelles.

Le portail modifie une valeur avec `commands/set { "id": <id>, "val": <value> }`.

---

## Ajouter un réglage (paramètre NVS)

```yaml
- id: my_param
  type: value
  role: my.param        # facultatif : un libellé du contrat
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # clé NVS (≤ 15 caractères)
  persist: true
  scope: global
  default: 50
```

`bind` = clé NVS. `persist: true` = la valeur survit à un redémarrage.

`role:` n'est pas un champ libre : la valeur doit figurer dans `canonical_roles` du contrat, sinon la compilation échoue. La liste est dans `contracts/mqtt_contract.yaml` → `canonical_roles` ou dans `menu.template.yaml`. Un nouveau rôle s'ajoute d'abord au contrat, puis `regen.sh`.

---

## Ajouter une opération à la carte de l'appareil

Les opérations (lancer, arrêter, chauffer, éclairer) se déclarent comme actions de la carte, pas comme éléments du menu. Leurs limites peuvent venir d'un élément du menu :

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

La description complète : [Carte de l'appareil : le card manifest](../09-add-product/02-add-widget.md).

---

## À NE PAS faire

- N'ajoutez pas `widget:` dans `menu.yaml`. Le champ `widget` de `canonical_roles` est une donnée de référence : ni le portail ni l'application ne le lisent.
- Ne modifiez pas `mqtt-api.types.ts` à la main : il est généré par `regen.sh`.
- Ne touchez pas aux flags `Config.hasXxx` pour de nouvelles actions : ils servent uniquement à la télémétrie (capteurs, états).
