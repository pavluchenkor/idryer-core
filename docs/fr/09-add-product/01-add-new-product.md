---
title: "Ajouter un nouveau produit basé sur idryer-core"
description: "Liste de contrôle pour un nouvel appareil iDryer sur la façade iDryer::Link : projet, firmware minimal, Wi-Fi et association dans l'application, télémétrie, menu de réglages, carte de l'appareil, contrat."
---

# Ajouter un nouveau produit basé sur idryer-core

Utilisez ce guide quand vous construisez un nouveau produit sur `idryer-core` : séchoir à filament, bloc chauffant, éclairage, capteur ou autre module. Il montre ce que le core fait pour vous et ce que le code du produit doit ajouter.

Un exemple complet qui compile est l'armoire de stockage chauffée de la documentation Build-Your-Own-iDryer (`example/09-cabinet`) : il parcourt tout ce qui figure sur cette page.

---

## Sur quoi repose un produit

Un produit parle au core par un seul objet — la façade `iDryer::Link` (`<iDryer.h>`). Dans `s_link.begin()` et `s_link.loop()`, le core :

- reçoit le réseau Wi-Fi de l'application iDryer par les ondes (ESPTouch) ou de l'installateur web par USB (Improv) et maintient la connexion ;
- associe l'appareil à un compte : attend un jeton d'association à usage unique — venant de l'application par le réseau local ou par le port série (`PAIR_TOKEN:<jeton>`) — et l'échange sur le portail contre un secret permanent ;
- se connecte à MQTT et publie `telemetry` et `status` selon les périodes de `Config` ;
- s'annonce sur le réseau local (mDNS `_idryer._tcp`) et accepte les commandes de l'application par WebSocket ;
- publie le card manifest de la carte de l'appareil.

Les classes de plus bas niveau (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` et d'autres) sont des éléments internes du core : le jeton d'association n'atteint la partie cloud qu'à l'intérieur d'`iDryer::Link`. Construisez le produit sur la façade.

---

## 1. Projet

`idryer-core` va dans `lib/idryer-core/` (copie ou lien symbolique) ; PlatformIO prend les bibliothèques du core dans son `library.json`. `platformio.ini` minimal :

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; transport ESP8266 des dépendances d'espMqttClient : ne compile pas sur ESP32
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Sans `lib_ignore = ESPAsyncTCP`, la compilation échoue dans `ESPAsyncTCP.cpp` ; sans `MQTT_BROKER` et `MQTT_PORT`, le core ne compile pas.

---

## 2. Firmware minimal

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // pas un produit iDryer : la carte vient du manifeste
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // Appareil dissocié sur le portail : effacer le secret, attendre une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // vos fonctions de capteurs
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` démarre le Wi-Fi, l'association, MQTT et l'accès local ; `s_link.loop()` doit tourner en permanence, sans `delay()`. La commande `revoke` vient du portail quand l'appareil est dissocié du compte : `handleRevoke()` efface le secret, et l'appareil attend un nouveau jeton d'association.

---

## 3. Wi-Fi et association — rien dans le code

Le firmware ne contient ni le mot de passe du réseau ni les données du compte. L'utilisateur connecte l'appareil dans l'application iDryer : **Connecter un nouvel appareil** → étape **Wi-Fi** (l'application envoie le réseau par ESPTouch) → étape **Association** (l'application trouve l'appareil par mDNS, obtient un jeton à usage unique auprès du portail et le remet à l'appareil ; l'appareil active lui-même le jeton sur le portail). Tant que le Wi-Fi n'est pas établi, le port série reste muet : le core le réserve à l'installateur web (Improv).

Pas à pas, avec le journal attendu — Build-Your-Own-iDryer, chapitre « Démarrage du firmware sur le core ».

---

## 4. Données : télémétrie et statut

- Les flags `has*` de `Config` définissent quels champs du vocabulaire partent dans la télémétrie et quelles cellules apparaissent sur la carte.
- Écrivez les valeurs dans `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) et `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`) ; le core les publie selon les périodes de `Config`, et `s_link.publishStatusNow()` envoie le statut tout de suite.
- Un champ à vous — via `s_link.onTelemetryPublish()`, sur la carte — via `s_link.card().sensor()` ; voir [Carte de l'appareil](02-add-widget.md).

---

## 5. Réglages : le menu

Les réglages sont décrits dans `src/menu/menu.yaml` ; le générateur `menu_gen.py` en tire du code C++, le stockage en NVS et le JSON du menu ([Le menu comme protocole](../08-contracts/02-menu-as-protocol.md)). Le code du produit :

- charge le menu avant `s_link.begin()` : `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()` ;
- le publie avec `menu_buildFullJson()` et `s_link.devicePublisher()->publishConfigRaw()` — au passage en ligne et sur la commande `get_config` ;
- applique la commande `set` avec `menu_apply_by_bind()` (valeur, NVS et cache d'un coup) et republie le menu.

Le code complet — Build-Your-Own-iDryer, chapitre « Menu depuis YAML ».

---

## 6. Carte de l'appareil

Ce que montre la carte et les opérations qu'elle lance se déclarent avec `s_link.card()` — [Carte de l'appareil : le card manifest](02-add-widget.md).

---

## 7. Contrat

Quand vous ajoutez des topics ou modifiez des payloads :

1. mettez à jour `contracts/mqtt_contract.yaml` ;
2. lancez `contracts/regen.sh` et committez les fichiers générés.

---

## Appareils à deux puces

Pour un ESP32 qui travaille avec un contrôleur séparé (par exemple RP2040) par UART, le core fournit le pont UART `idryer_uart.h` ; la référence qui fonctionne est le firmware `idryer-link`.
