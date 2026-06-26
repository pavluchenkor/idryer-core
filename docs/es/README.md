---
title: "idryer-core para dispositivos iDryer y módulos DIY"
description: "Documentación de idryer-core: conectar un dispositivo ESP32, MQTT, Wi-Fi, máquina de estados en la nube y comandos para un secador de filamento o módulo de impresora 3D."
---

# idryer-core para dispositivos iDryer y módulos DIY

`idryer-core` es útil cuando un secador de filamento DIY, una cámara calefactada, un sistema de iluminación u otro módulo de impresora 3D debe convertirse en un dispositivo iDryer gestionado. La biblioteca se encarga de Wi-Fi, MQTT, comandos, telemetría y comunicación con el portal, mientras que el código del producto describe el comportamiento del dispositivo concreto.

`idryer-core` es una biblioteca C++ (Arduino/PlatformIO) para dispositivos iDryer basados en ESP32. Gestiona WiFi, MQTT, la máquina de estados en la nube y el enrutamiento de comandos. El producto implementa solo el comportamiento específico del dispositivo.

Esta es la documentación de la **biblioteca**, no de un producto específico.
La documentación de producto está en [`docs/ru/`](../../docs/ru/).

---

## Inicio rápido

**Tres cosas que implementas:**

1. Implementar `IProfile` — cinco métodos (configuración, información, loop).
2. Ensamblar `main.cpp` — objetos estáticos y dependencias pasadas por constructores.
3. Registrar `handleCommand` — un único manejador para MQTT y, opcionalmente, WS local.

**Tres cosas que hace la biblioteca:**

1. Gestiona WiFi → provisioning → sesión MQTT.
2. Enruta los comandos entrantes a tu `handleCommand` (`ping` se procesa internamente).
3. Llama a tus métodos de `IProfile` en los momentos correctos.

**Lo que puedes dejar sin tocar:**

- `ArduinoWifiManager`, `ArduinoCredentialStore` y otras clases `Arduino*` — úsalas tal cual, sin subclases.
- `CloudStateMachine` — créala y pásala a `IdryerRuntime`; a partir de ahí se gestiona sola.
- `ActionDispatcher` — fallback de compatibilidad para invoke/set; para un producto nuevo, el manejo de comandos pasa por `setCommandHandler()`, no por `ActionDispatcher`.

Guía práctica: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

Ejemplos funcionales: [`examples/`](../../examples/)

---

## Secciones

| Sección | Descripción |
|---------|-------------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | Propósito de la biblioteca, qué no hace y quién la usa |
| [01-overview/02-module-map](01-overview/02-module-map.md) | Tabla de todos los módulos: propósito y opcionalidad |
| [02-getting-started](02-quickstart/01-five-minutes.md) | Entrada breve para un desarrollador nuevo: qué conectar, flashear y esperar |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | Cómo el producto ensambla el stack: orden de creación de objetos y patrón de `main.cpp` |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | Qué vive en la biblioteca y qué vive en el producto |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | Flujo de datos en un dispositivo en ejecución: comandos entrantes, mensajes salientes, conexiones |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | Clase `MqttClient`: constructor, conexión y publicación |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | Todos los topics MQTT: cadenas, payloads, retained, QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`: qué coordina y qué comandos maneja |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | Puente UART para dispositivos con dos MCU |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu, Home Assistant, Moonraker: configuración y limitaciones |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | Implementaciones Arduino de las interfaces de dispositivo |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | Interfaz `IProfile`, callbacks, ejemplo `LedStripProfile` |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`: propósito y reglas de modificación |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | Checklist para crear un producto nuevo sobre `idryer-core` |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | Problemas comunes: WiFi, provisioning, MQTT, comandos, LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | Cómo añadir un sensor y publicar sus lecturas |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | Cómo añadir un periférico y recibir comandos |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | Cómo añadir un transporte paralelo (BLE, HTTP, personalizado) |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | Recetas aplicadas para pasar datos entre sensores, periféricos, perfil y publicadores |
