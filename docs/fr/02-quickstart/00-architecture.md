# Comment fonctionne idryer-core

idryer-core est une bibliothèque pour ESP32 qui gère l'ensemble de la pile cloud : approvisionnement en WiFi via Improv-Serial, protocole de réclamation pour lier un appareil à un compte idryer.org, session MQTT TLS avec reconnexion automatique, routage des commandes du portail et publication périodique de la télémétrie.

Vous n'écrivez que ce qui est spécifique à votre appareil : lire les capteurs, piloter les périphériques. Tout le reste se trouve dans la bibliothèque.

## mqtt_contract.yaml — source unique de vérité

Le fichier [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) définit :

- **capacités** — quels périphériques chaque type d'appareil supporte (radiateur, bande LED, capteurs) ;
- **champs de télémétrie** — noms de champs et types de données dans les paquets MQTT ;
- **protocole UART** — structures entre ESP32 et un co-processeur ;
- **types TypeScript** — pour le frontend du portail.

À partir de ce fichier, le code est généré automatiquement :

| Ce qui est généré | Où |
|---|---|
| `iDryer::Config` (drapeaux has*) | `src/_generated/iDryer_api.h` |
| Sujets MQTT (constantes C++) | `contracts/_generated/mqtt_topics.h` |
| Types TypeScript | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Ne modifiez pas les fichiers dans `src/_generated/` et `contracts/_generated/` manuellement — ils sont écrasés lors du prochain cycle de régénération.

## Comment ajouter de nouveaux périphériques

La procédure est la même pour toute nouvelle capacité — un bouton, un capteur CO2, un lecteur RFID.

**1.** Ajoutez une entrée à `capability_vocabulary` dans [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) :

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** Exécutez la régénération :

```bash
cd contracts
./regen.sh
```

Après cela, `iDryer::Config` aura un champ `hasCo2`, et TypeScript aura `HardwareUnitConfigCapabilities.co2`.

**3.** Définissez le drapeau dans `main.cpp` de votre appareil :

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Flashez l'appareil. Le portail lira `co2: true` dans le sujet MQTT `/info` et affichera le bloc d'interface utilisateur correspondant automatiquement — aucune modification du côté du portail requise.

Pour les types de périphériques pas encore dans le contrat, ouvrez une PR au référentiel idryer-core avec une entrée à `capability_vocabulary`. Après la fusion — exécutez `regen.sh`.

## Deux produits de production construits sur cette bibliothèque

**iDryer Storage Link** — ESP32-C3 avec une bande LED WS2812B et un capteur de température/humidité SHT31.

**iHeater Link** — ESP32-C3 avec sortie RMT vers le radiateur iHeater, avec intégrations pour Bambu Lab, Klipper/Moonraker et Home Assistant.

Les deux produits incluent idryer-core via PlatformIO `lib_deps` et n'implémentent que leur logique spécifique au produit.

## Prochaines étapes

- [01-wifi.md](01-wifi.md) — connectez ESP32 à WiFi en utilisant Improv-Serial.
- [../../../README.md](../../../README.md) — aperçu de la bibliothèque et référence de génération de code.
