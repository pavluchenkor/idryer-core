---
title: "Carte de l'appareil : le card manifest"
description: "Comment un firmware sur idryer-core décrit sa carte avec s_link.card() : capteurs, actions avec paramètres de lancement, ce qui part vers le portail et ce que dessinent le portail et l'application."
---

# Carte de l'appareil : le card manifest

Le portail et l'application mobile construisent la carte de n'importe quel appareil à partir de son **card manifest** — une description que le firmware publie sur lui-même : quoi afficher et ce qui peut être commandé. Un nouveau type d'appareil ne demande aucun code dans le portail ni dans l'application.

Le manifeste fait partie de la façade `iDryer::Link` (`s_link.card()`). Le runtime bas niveau (`IdryerRuntime`) ne le publie pas.

Le menu et la carte sont deux choses différentes. Le menu reflète les réglages de l'appareil : une valeur modifiée depuis le portail est écrite dans la mémoire de l'appareil. La carte affiche des mesures et lance des opérations : les paramètres de lancement partent avec une commande et ne sont pas écrits dans le menu. Un paramètre de lancement peut prendre ses limites et sa valeur par défaut dans un élément du menu.

---

## Fonctionnement

```text
firmware : déclarations s_link.card()
   │  le core construit le JSON et le publie en retained (QoS 1) sur idryer/{key}/card
   ▼
backend du portail : valide le manifeste (limites, types et champs autorisés), l'enregistre
   ▼
portail et application : dessinent la carte
   │  l'utilisateur appuie sur un bouton
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
le core transmet "card.<id>" à votre callback
```

Le core publie le manifeste une fois la connexion MQTT établie et le republie quand la déclaration ou un élément du menu dont elle dépend change.

---

## Entités : quoi afficher

Les capteurs du vocabulaire de l'écosystème sont ajoutés d'après les flags de `Config`, vous ne les déclarez pas :

| Flag `Config` | Cellule de la carte |
|---|---|
| `hasAirTemp` | température de l'air |
| `hasAirHumidity` | humidité |
| `hasHeaterTemp` | température du chauffage |
| `hasHeater` | puissance de chauffe |
| `hasFan` | ventilateur marche / arrêt |
| `hasServo` | volet ouvert / fermé |
| `hasWeight` | modules de pesée (topic `weights`) |
| `hasRfid` | l'unité a un lecteur RFID |

Votre propre valeur : ajoutez-la à la télémétrie et déclarez un capteur avec son chemin JSON :

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Les commandes simples sont aussi des entités : `button`, `number`, `select`. La valeur part dès que l'utilisateur la modifie, le core appelle votre callback :

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Actions : opérations avec paramètres de lancement

Une action est une opération de l'appareil : lancer le séchage, chauffer, allumer l'éclairage, arrêter. Elle a un **mode** — le mode de l'unité après l'action (`status.units[].mode`) — et des **paramètres** avec un sens (`purpose`). La carte décide quoi afficher d'après le mode actuel de l'unité :

- le mode de l'unité correspond au mode d'une action → l'appareil est occupé par celle-ci : bloc de session et bouton Arrêt (l'action de mode `IDLE`) ;
- sinon → le formulaire de lancement ; plusieurs actions de lancement → un sélecteur de mode.

Exemple : une armoire de stockage chauffée sur un ESP32. La température cible est l'élément de menu `target_temp` (30–50 °C, 45 par défaut).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // déjà dans 30..50
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
    menu_sync_state_to_cache();     // valeurs du menu dans le cache lu par la carte
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

Ce que reçoit le callback :

- les nombres sont bornés aux limites de l'unité, un nombre absent est remplacé par la valeur par défaut ;
- `unit` — l'indice de l'unité d'après `unitId` (`"U1"` → 0) ; une commande pour une unité que l'appareil n'a pas est ignorée ;
- après un lancement, réglez `status.mode` et appelez `publishStatusNow()` : d'après le statut, la carte passe au bloc de session.

### API des actions

| Appel | Ce qu'il fait |
|---|---|
| `card.action(id, mode, cb)` | une action ; `mode` — mode de l'unité ensuite, `nullptr` — le mode ne change pas |
| `.param(id, purpose, MENU_ID)` | un nombre : limites, pas, valeur par défaut et unité issus de l'élément du menu |
| `.param(id, purpose, min, max, step, def[, unit])` | un nombre avec ses propres limites |
| `.ceiling(MENU_ID)` | la limite haute du paramètre précédent est la valeur de cet élément du menu (par exemple la température d'air maximale) |
| `.stages(id, "stages", MENU_ID)` | étapes du profil `[{temperature, ramp, hold}]`, secondes ; température d'étape dans les limites de l'élément du menu |
| `.select(id, purpose, options, count[, def])` | un choix dans une liste ; une valeur hors liste est remplacée par la valeur par défaut |
| `.color(id, purpose, "#FFFFFF")` | une couleur `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | boutons permanents de l'en-tête : localiser l'appareil, effacer les erreurs |
| `.name("ru", "…").name("en", "…")` | le nom de l'action, quand la carte ne connaît pas le mode |

Valeurs de `purpose` : `target_temperature`, `target_humidity`, `duration` (minutes, 0 — sans limite), `stages`, `start_stage` (à partir de 0), `effect`, `rgb_color`. La carte s'en sert pour les libellés des champs et pour les fonctions du portail : le préréglage de séchage remplit `target_temperature` et `duration` d'une action de mode `DRYING`, un profil de séchage remplit `stages`.

Les callbacks sont des fonctions ou des lambdas sans capture. Les chaînes de `name` et les options de sélection sont stockées comme pointeurs : utilisez des littéraux ou des tableaux statiques.

---

## Ce qui part vers le portail

L'armoire ci-dessus publie (`Config` : `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`) :

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

`limits` et `default` viennent de l'élément du menu. Avec `.ceiling()`, le paramètre reçoit aussi `max_by` — le titre de l'élément du menu qui a coupé la limite haute ; la carte le nomme quand une valeur dépasse la limite.

---

## Ce que dessinent le portail et l'application

Schéma, pas une capture d'écran. Au repos :

```text
┌─ DIY Storage Cabinet ─────────────── [Inactif] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                   │  ← température, humidité, chauffage
│ | 0 %     | arrêt           |                   │  ← puissance, ventilateur (gris : 0 / arrêt)
│ [Temp. 45 °C          ]  [Stockage]             │  ← action "storage"
└─────────────────────────────────────────────────┘
```

Après le lancement, l'appareil signale le mode `STORAGE` et la même carte affiche le bloc de session (température cible et durée de stockage) et le bouton Arrêt — l'action de mode `IDLE`.

Règles des deux côtés :

- une cellule est grise quand il n'y a pas de valeur ; la puissance, aussi à 0 ; le ventilateur et le volet, à l'état arrêt / fermé ;
- une valeur hors limites n'est pas remplacée : le lancement est bloqué et la raison s'affiche sous le formulaire ;
- avec un firmware ayant des actions dans le manifeste, les boutons localiser et effacer les erreurs n'existent que s'il déclare des actions `identify` / `clear_errors`.

Où apparaît la carte :

| | Portail | Application |
|---|---|---|
| un type d'appareil qui n'est pas un produit iDryer | carte du tableau de bord issue du manifeste, avec ses actions ; page de l'appareil — mesures et actions (si le manifeste a des actions) | accueil — mesures ; actions — sur la page de l'appareil |
| `deviceType = Dryer` | la carte du séchoir avec les mêmes actions du manifeste | accueil — surveillance ; actions — sur la page de l'appareil |

---

## Limites

| | Core | Backend du portail |
|---|---|---|
| entités | 16 déclarées + automatiques | 32 |
| lignes de layout | 8 | 16 |
| ids par ligne | 4 | 4 |
| actions | 8 | 8 |
| paramètres par action | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| langues de `name` | `ru`, `en` | jusqu'à 4 |

`id` : lettres latines minuscules, chiffres et `_`, jusqu'à 24 caractères ; entités et actions partagent un même espace de noms. Le document du manifeste est limité à 4096 octets ; s'il ne tient pas, le core écrit `manifest overflows` dans le journal et ne le publie pas.

Le format lui-même : `contracts/mqtt_contract.yaml`, section `mqtt_only`, `suffix: card`.
