---
title: "idryer-core pour les appareils iDryer et les modules DIY"
description: "Documentation idryer-core : connecter un appareil ESP32, MQTT, Wi-Fi, machine d'état cloud et commandes pour un séchoir à filament ou un module d'imprimante 3D."
---

# idryer-core pour les appareils iDryer et les modules DIY

`idryer-core` est utile lorsqu'un séchoir à filament DIY, une chambre chauffée, un système d'éclairage ou un autre module d'imprimante 3D doit devenir un appareil iDryer géré. La bibliothèque prend en charge le Wi-Fi, MQTT, les commandes, la télémétrie et la communication avec le portail, tandis que le code produit décrit le comportement de l'appareil concret.

`idryer-core` est une bibliothèque C++ (Arduino/PlatformIO) pour les appareils iDryer basés sur ESP32. Elle gère le WiFi, MQTT, la machine d'état cloud et le routage des commandes. Le produit n'implémente que le comportement propre à l'appareil.

Ceci est la documentation de la **bibliothèque**, pas celle d'un produit particulier.
La documentation produit se trouve dans [`docs/ru/`](../../docs/ru/).

---

## Démarrage rapide

**Trois choses que vous implémentez :**

1. Implémenter `IProfile` — cinq méthodes (configuration, information, loop).
2. Assembler `main.cpp` — objets statiques, dépendances passées par constructeurs.
3. Enregistrer `handleCommand` — un gestionnaire unique pour MQTT et éventuellement le WS local.

**Trois choses que fait la bibliothèque :**

1. Gère WiFi → provisioning → session MQTT.
2. Route les commandes entrantes vers votre `handleCommand` (`ping` est traité en interne).
3. Appelle vos méthodes `IProfile` aux bons moments.

**Ce que vous pouvez laisser intact :**

- `ArduinoWifiManager`, `ArduinoCredentialStore` et les autres classes `Arduino*` — utilisez-les telles quelles, sans sous-classe.
- `CloudStateMachine` — créez-la et passez-la à `IdryerRuntime`; elle se gère ensuite elle-même.
- `ActionDispatcher` — fallback de compatibilité pour invoke/set ; pour un nouveau produit, le traitement des commandes passe par `setCommandHandler()`, pas par `ActionDispatcher`.

Guide pratique : [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Exemples fonctionnels : [`examples/`](../../examples/)

---

## Sections

| Section | Description |
|---------|-------------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Objectif de la bibliothèque, ce qu'elle ne fait pas, qui l'utilise |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Tableau de tous les modules : objectif et caractère optionnel |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Entrée rapide pour un nouveau développeur : quoi câbler, flasher et vérifier |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | Comment le produit assemble la pile : ordre de création des objets, modèle `main.cpp` |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | Ce qui appartient à la bibliothèque et ce qui appartient au produit |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Flux de données dans un appareil en cours d'exécution : commandes entrantes, messages sortants, connexions |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | Classe `MqttClient` : constructeur, connexion, publication |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | Tous les topics MQTT : chaînes, payloads, retained, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime` : ce qu'elle coordonne et les commandes qu'elle traite |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | Pont UART pour appareils à deux MCU |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker : configuration et limites |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Implémentations Arduino des interfaces d'appareil |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | Interface `IProfile`, callbacks, exemple `LedStripProfile` |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml` : objectif et règles de modification |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Checklist pour créer un nouveau produit sur `idryer-core` |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | Problèmes courants : WiFi, provisioning, MQTT, commandes, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | Ajouter un capteur et publier ses mesures |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | Ajouter un périphérique et recevoir des commandes |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | Ajouter un transport parallèle (BLE, HTTP, personnalisé) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Recettes appliquées pour passer des données entre capteurs, périphériques, profil et publisheurs |
