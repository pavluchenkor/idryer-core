# Configuration détaillée

La voie courte est [Démarrer en 5 minutes](01-five-minutes.md). Cette page couvre l'environnement, les options de compilation, les journaux et le mode développement.

## Le noyau dans le projet

Le noyau se trouve dans `lib/idryer-core` du projet PlatformIO : en copie, en sous-module git ou en lien symbolique vers un clone commun. Son `library.json` apporte les dépendances : MQTT, ArduinoJson, WebSockets, Improv. Le `lib_deps` du projet ne contient que les bibliothèques de vos capteurs.

Cartes des produits sur le noyau : ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP est le transport ESP8266 des dépendances d'espMqttClient : il ne compile pas sur ESP32.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Carte sans USB-UART (ESP32-C3 SuperMini) : Serial passe par USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Option | Rôle |
|---|---|
| `IDRYER_API_BASE` | adresse de l'API du portail : activation, association |
| `MQTT_BROKER`, `MQTT_PORT` | le broker du portail |
| `MQTT_USE_TLS=1` | connexion sécurisée au broker |
| `lib_ignore = ESPAsyncTCP` | transport ESP8266 des dépendances du client MQTT : ne compile pas sur ESP32 |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial par USB sur les cartes sans USB-UART |

Les macros de chaîne ont besoin de guillemets à l'extérieur et à l'intérieur.

## Périodes de publication

Les champs `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs` ; zéro signifie la valeur du contrat :

| Quoi | En fonctionnement | Au repos |
|---|---|---|
| télémétrie | 30 s | 60 s |
| état | tout de suite à un changement de mode, de consigne ou de durée ; rapprochement toutes les 60 s | rapprochement toutes les 5 min |

« Au repos » signifie qu'aucune unité n'est dans un mode actif.

## Journaux

```bash
pio device monitor -b 115200
```

Tant que l'appareil n'a pas de réseau, le journal reste muet : le port est occupé par Improv. Les journaux s'activent avec la ligne `[BOOT] WiFi ok, logs enabled`. Ensuite, les commandes `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` fonctionnent : voir [Association au compte](02-claim.md).

## Mode développement : `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Avec l'option :

- les journaux partent vers le port dès la mise sous tension ;
- Improv et les commandes `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` sont désactivés : votre code lit les lignes entrantes du port ;
- le réseau vient de l'application (ESPTouch) ou du code : `seedWifiCredentialsIfEmpty()` avant `begin()`, voir [Wi-Fi](01-wifi.md).

Le firmware publié se compile sans l'option.

## Suite

- [Exemples du noyau](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Ajouter un nouveau produit](../09-add-product/01-add-new-product.md).
- [Carte de l'appareil : le card manifest](../09-add-product/02-add-widget.md).
