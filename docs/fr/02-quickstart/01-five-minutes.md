---
title: "Démarrer un appareil sur idryer-core en 5 minutes"
description: "Le premier appareil sur idryer-core : projet PlatformIO, firmware minimal, Wi-Fi et association au compte dans l'application iDryer."
---

# Démarrer un appareil sur idryer-core en 5 minutes

Après cette page, l'ESP32 est sur le réseau, associé à votre compte et visible sur le portail [portal.idryer.org](https://portal.idryer.org/) et dans l'application iDryer. Il vous faut : une carte ESP32-C3 (DevKit, Super Mini ou compatible), un câble USB, PlatformIO dans VS Code, un téléphone avec l'application iDryer, un réseau Wi-Fi 2,4 GHz.

## 1. Projet PlatformIO

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← copie, sous-module git ou lien symbolique
└── src/
    └── main.cpp
```

`platformio.ini` :

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

Les bibliothèques du noyau (MQTT, ArduinoJson, WebSockets, Improv) arrivent d'elles-mêmes depuis le `library.json` du noyau. Une carte avec USB-UART n'a pas besoin des deux dernières options ; réglez `board` pour votre carte.

## 2. Code

Copiez l'exemple [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) dans `src/main.cpp` :

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // votre appareil
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Période de clignotement selon l'état ; 0 : ne pas clignoter.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Dissociation : le noyau efface le secret et attend une nouvelle association.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // Le noyau publie lui-même les champs de télémétrie, toutes les 30 s (toutes les 60 s au repos).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

Le firmware ne contient ni mot de passe du réseau ni données de compte : l'appareil reçoit le réseau et l'association depuis l'application.

## 3. Flasher et ouvrir le journal

```bash
pio run -t upload
pio device monitor -b 115200
```

Tant que l'appareil n'a pas de Wi-Fi, le journal reste muet : le noyau garde le port pour Improv. La LED clignote vite : l'appareil attend un réseau et une association.

## 4. Wi-Fi et association dans l'application

1. Connectez le téléphone au réseau Wi-Fi où fonctionnera l'appareil et connectez-vous à l'application iDryer avec votre compte du portail.
2. Sur l'écran d'accueil, touchez **Connecter un nouvel appareil** : l'étape **Wi-Fi** s'ouvre.
3. Vérifiez le nom du réseau, saisissez le mot de passe et touchez **Connecter l’appareil**. L'application envoie les réglages pendant 90 secondes au maximum ; quand l'appareil rejoint le réseau, **Appareil connecté** s'affiche. Touchez **Suivant**.
4. À l'étape **Association**, touchez **Associer**. L'application trouve l'appareil sur le réseau, obtient du portail un jeton d'association à usage unique et le remet à l'appareil.
5. Après **Appareil associé**, l'appareil apparaît dans la liste des appareils du portail et de l'application.

Le journal après l'arrivée sur le réseau :

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Après l'association :

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Vérification

- l'appareil est en ligne sur le portail et dans l'application ;
- la carte affiche la température de la puce ESP32 ;
- la LED clignote une fois par seconde. Allumée ou éteinte en continu : pas de liaison avec le portail.

## Si cela n'a pas marché

- l'application n'a pas vu l'appareil se connecter : vérifiez le mot de passe et que le réseau est en 2,4 GHz ; avec un mauvais mot de passe, l'appareil attend de nouveau les réglages. Autres façons de transmettre le réseau : [Wi-Fi](01-wifi.md) ;
- l'étape **Association** n'a pas trouvé l'appareil : le téléphone et l'appareil doivent être sur le même réseau ; les réseaux invités bloquent souvent la découverte des appareils. Plus : [Association au compte](02-claim.md) ;
- compilation et options : [Configuration détaillée](99-detailed-setup.md).

## Suite

- [Télémétrie](03-telemetry.md) : un capteur et une valeur à vous sur la carte.
- [Exemples du noyau](https://github.com/pavluchenkor/idryer-core/tree/main/examples) : actions de carte, capteurs et commandes à vous.
