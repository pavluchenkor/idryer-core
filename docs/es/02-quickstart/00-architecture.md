# Cómo funciona idryer-core

idryer-core es una biblioteca para ESP32. Se encarga de todo lo que une un dispositivo con el portal y la aplicación:

- Wi-Fi: la red llega desde la aplicación iDryer (ESPTouch) o desde una página web por USB (Improv);
- la vinculación con una cuenta mediante un token de un solo uso, y la desvinculación;
- una sesión MQTT segura con reconexión;
- acceso por red local: la aplicación controla el dispositivo incluso sin internet;
- la publicación de telemetría y estado, la entrega de órdenes;
- la actualización del firmware por aire;
- la tarjeta del dispositivo en el portal y en la aplicación.

Tú escribes solo tu parte: leer sensores, manejar cargas, declarar qué muestra la tarjeta y qué operaciones inicia.

## Un único punto de entrada: `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // tu propio dispositivo
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // celda de temperatura
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
    s_link.telemetry.airTempC[0] = readTemperature();   // tu código
}
```

| Tú | El núcleo |
|---|---|
| rellenas `s_link.telemetry.*` | publica cada 30 s, cada 60 s en reposo |
| cambias `s_link.status.*` (modo, consigna, tiempo) y llamas a `publishStatusNow()` | entrega el estado; la tarjeta cambia según el modo |
| declaras `s_link.card()`: sensores, controles, acciones | construye el card manifest y lo publica |
| registras `s_link.onCommand(...)` | pasa las órdenes de la nube y de la red local |

## Tarjeta del dispositivo

El portal y la aplicación dibujan la tarjeta a partir del card manifest que envía el dispositivo:

- las banderas `Config.has*` dan celdas listas: temperatura, humedad, potencia de calefacción, ventilador y otras;
- `card().sensor(...)` añade un valor propio por su ruta en la telemetría;
- `card().action(...)` declara una operación: el modo de la unidad después de ella y sus parámetros de inicio.

No hace falta código en el portal. Detalles: [Tarjeta del dispositivo: el card manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml`, la fuente de verdad

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) describe el protocolo: topics, campos de telemetría y de estado, capacidades de los dispositivos, el card manifest. A partir de él se generan:

| Qué | Dónde |
|---|---|
| `iDryer::Config` (banderas `has*`) y estructuras de la API | `src/_generated/iDryer_api.h` |
| topics MQTT | `contracts/_generated/mqtt_topics.h` |
| protocolo UART ESP32 ↔ controlador | `contracts/_generated/uart_protocol.h` |
| tipos TypeScript para el portal | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    No edites a mano los archivos de `_generated/`: `contracts/regen.sh` los reescribe desde el contrato.

Un valor propio no requiere cambiar el contrato: lo declara `card().sensor(...)`. El contrato cambia cuando todos los productos necesitan una capacidad nueva: primero `mqtt_contract.yaml`, luego `regen.sh`, luego el código.

## Productos sobre el núcleo

- **iDryer Link**: módulo de comunicación del secador, un ESP32 junto al controlador, intercambio por UART.
- **iDryer Storage**: iluminación de una estantería de bobinas, tira direccionable y sensor SHT31.
- **iHeater Link**: control del calefactor iHeater, integraciones con Bambu Lab, Klipper/Moonraker y Home Assistant.

## Siguiente

[Arrancar en 5 minutos](01-five-minutes.md).
