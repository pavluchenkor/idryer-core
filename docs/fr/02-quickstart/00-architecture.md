# Comment fonctionne idryer-core

idryer-core est une bibliothèque pour ESP32. Elle prend en charge tout ce qui relie un appareil au portail et à l'application :

- Wi-Fi : le réseau vient de l'application iDryer (ESPTouch) ou d'une page web par USB (Improv) ;
- l'association à un compte par jeton à usage unique, et la dissociation ;
- une session MQTT sécurisée avec reconnexion ;
- l'accès en réseau local : l'application pilote l'appareil même sans internet ;
- la publication de la télémétrie et de l'état, la livraison des commandes ;
- la mise à jour du firmware à distance ;
- la carte de l'appareil sur le portail et dans l'application.

Vous n'écrivez que votre partie : lire les capteurs, piloter les charges, déclarer ce que montre la carte et quelles opérations elle lance.

## Un seul point d'entrée : `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // votre appareil
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // cellule de température
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
    s_link.telemetry.airTempC[0] = readTemperature();   // votre code
}
```

| Vous | Le noyau |
|---|---|
| remplissez `s_link.telemetry.*` | publie toutes les 30 s, toutes les 60 s au repos |
| modifiez `s_link.status.*` (mode, consigne, durée) et appelez `publishStatusNow()` | transmet l'état ; la carte change selon le mode |
| déclarez `s_link.card()` : capteurs, commandes, actions | construit le card manifest et le publie |
| enregistrez `s_link.onCommand(...)` | transmet les commandes du cloud et du réseau local |

## Carte de l'appareil

Le portail et l'application dessinent la carte d'après le card manifest envoyé par l'appareil :

- les indicateurs `Config.has*` donnent des cellules toutes prêtes : température, humidité, puissance de chauffe, ventilateur et d'autres ;
- `card().sensor(...)` ajoute une valeur à vous par son chemin dans la télémétrie ;
- `card().action(...)` déclare une opération : le mode de l'unité après elle et ses paramètres de lancement.

Aucun code n'est nécessaire sur le portail. Détails : [Carte de l'appareil : le card manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml`, la source de vérité

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) décrit le protocole : topics, champs de télémétrie et d'état, capacités des appareils, card manifest. Il génère :

| Quoi | Où |
|---|---|
| `iDryer::Config` (indicateurs `has*`) et structures de l'API | `src/_generated/iDryer_api.h` |
| topics MQTT | `contracts/_generated/mqtt_topics.h` |
| protocole UART ESP32 ↔ contrôleur | `contracts/_generated/uart_protocol.h` |
| types TypeScript pour le portail | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Ne modifiez pas à la main les fichiers de `_generated/` : `contracts/regen.sh` les réécrit depuis le contrat.

Une valeur à vous ne demande aucune modification du contrat : `card().sensor(...)` la déclare. Le contrat change quand tous les produits ont besoin d'une nouvelle capacité : d'abord `mqtt_contract.yaml`, puis `regen.sh`, puis le code.

## Produits sur le noyau

- **iDryer Link** : module de communication du séchoir, un ESP32 à côté du contrôleur, échange par UART.
- **iDryer Storage** : éclairage d'une étagère de bobines, ruban adressable et capteur SHT31.
- **iHeater Link** : pilotage du chauffage iHeater, intégrations avec Bambu Lab, Klipper/Moonraker et Home Assistant.

## Suite

[Démarrer en 5 minutes](01-five-minutes.md).
