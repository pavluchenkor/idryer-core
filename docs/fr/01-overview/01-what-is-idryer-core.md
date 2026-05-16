# Qu'est-ce que idryer-core

Si vous construisez un appareil ESP32 pour le cloud iDryer, cette bibliothèque gère l'approvisionnement en WiFi (Improv), le protocole de réclamation, la session MQTT (TLS, reconnexion, synchronisation horaire), la publication périodique de la télémétrie/statut et le routage des commandes entrantes. Environ 500 lignes de code passe-partout se réduisent à `link.begin(); link.loop();`.

## Exemple minimal

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## Ce que fait la bibliothèque

- Connexion WiFi et maintien actif ; approvisionnement en Improv sur Web Serial pour la configuration initiale.
- Protocole de réclamation : enregistrement de l'appareil dans le backend, réclamation du compte via PIN.
- Session MQTT avec le courtier iDryer : TLS, session persistante, reconnexion automatique, synchronisation horaire NTP.
- Publication périodique de la télémétrie (`Telemetry`) et du statut (`Status`) selon un minuteur.
- Routage des commandes entrantes (`commands/invoke`, `commands/set`, `commands/ping`) vers le gestionnaire de produits.
- Serveur WebSocket local : un client LAN voit le même flux que le cloud.
- Persistance NVS : identifiants WiFi, jeton d'appareil, configuration de menu sur les redémarrages.

## Ce que la bibliothèque ne fait pas

- Ne gère pas le matériel du produit : ventilateurs, radiateurs, bandes LED, capteurs.
- Ne contient pas la logique métier pour le séchage, le stockage ou l'éclairage.
- Ne connaît pas les paramètres de menu spécifiques au produit — elle les transporte simplement.
- Ne publie pas la télémétrie sans données du produit : vous remplissez `link.telemetry.*` vous-même dans `loop()`.

## Où aller ensuite

- [Commencer en 5 minutes](../02-quickstart/01-five-minutes.md)
- [API complète : iDryer::Link](../03-public-api/01-link-api-reference.md)
