# Commencer en 5 minutes

Après cette page, votre ESP32 sera flashé, se connectera à WiFi et apparaîtra dans [portal.idryer.org](https://portal.idryer.org/) avec le statut En ligne. Configuration requise : ESP32-C3 (DevKit, Super Mini ou compatible), câble USB, PlatformIO dans VS Code.

## 1. Préparez secrets.h

Copiez [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) vers `include/secrets.h` dans votre projet et définissez votre SSID WiFi et votre mot de passe (2,4 GHz uniquement) :

```cpp
#define WIFI_SSID      "votre-ssid"
#define WIFI_PASSWORD  "votre-mot-de-passe"
```

Ajoutez `include/secrets.h` à `.gitignore`.

## 2. Configurez platformio.ini

Créez `platformio.ini` à la racine du projet :

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://path/to/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

Modifiez `board` pour correspondre à votre carte. Remplacez `path/to/idryer-core` par le chemin réel de la bibliothèque.

## 3. Copiez l'exemple 01_blink_status

Copiez le contenu de [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) dans `src/main.cpp` de votre projet. L'exemple ne nécessite aucun capteur ni dépendance supplémentaire — juste une racine de composition minimale.

## 4. Flash

```bash
pio run -e blink-demo -t upload
```

## 5. Ouvrez le moniteur série

```bash
pio device monitor -b 115200
```

Séquence de journal attendue :

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

Après avoir saisi le PIN dans le portail (étape 6) :

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Si l'appareil s'est arrêté au message `PIN: ...` — c'est prévu ; passez à l'étape 6.

## 6. Réclamez l'appareil dans le portail

Ouvrez [portal.idryer.org](https://portal.idryer.org/), allez sur **Ajouter un appareil** et saisissez le PIN du moniteur série. Après une réclamation réussie, l'appareil passera à `En ligne` et la LED intégrée clignotera toutes les 500 ms.

Flux de réclamation détaillé : [Intégration](02-onboarding.md).

## Que faire ensuite

- Ajouter un capteur — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- Ajouter un périphérique — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- Référence API complète — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Comment ça marche en interne — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
