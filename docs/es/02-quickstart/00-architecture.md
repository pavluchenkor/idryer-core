# Cómo funciona idryer-core

idryer-core es una biblioteca para ESP32 que maneja el stack de nube completo: aprovisionamiento de WiFi a través de Improv-Serial, protocolo de reclamación para vincular un dispositivo a una cuenta idryer.org, sesión MQTT TLS con reconexión automática, enrutamiento de comandos desde el portal y publicación periódica de telemetría.

Solo escribes lo específico para tu dispositivo: lectura de sensores, control de periféricos. Todo lo demás está dentro de la biblioteca.

## mqtt_contract.yaml — fuente única de verdad

El archivo [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) define:

- **capacidades** — qué periféricos soporta cada tipo de dispositivo (calentador, tira LED, sensores);
- **campos de telemetría** — nombres de campos y tipos de datos en paquetes MQTT;
- **protocolo UART** — estructuras entre el ESP32 y un coprocesador;
- **tipos TypeScript** — para el frontend del portal.

Desde este archivo, se genera código automáticamente:

| Qué se genera | Dónde |
|---|---|
| `iDryer::Config` (banderas has*) | `src/_generated/iDryer_api.h` |
| Tópicos MQTT (constantes C++) | `contracts/_generated/mqtt_topics.h` |
| Tipos TypeScript | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    No edites archivos en `src/_generated/` y `contracts/_generated/` manualmente — se sobrescriben en la siguiente ejecución de regeneración.

## Cómo agregar nuevos periféricos

El procedimiento es el mismo para cualquier nueva capacidad — un botón, un sensor de CO2, un lector RFID.

**1.** Agrega una entrada a `capability_vocabulary` en [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml):

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** Ejecuta la regeneración:

```bash
cd contracts
./regen.sh
```

Después de esto, `iDryer::Config` tendrá un campo `hasCo2`, y TypeScript tendrá `HardwareUnitConfigCapabilities.co2`.

**3.** Establece la bandera en tu `main.cpp`:

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Flasha el dispositivo. El portal leerá `co2: true` desde el tópico MQTT `/info` y mostrará el bloque de interfaz de usuario correspondiente automáticamente — no se requieren cambios en el portal.

Para tipos de periféricos no incluidos aún en el contrato, abre un PR al repositorio idryer-core agregando una entrada a `capability_vocabulary`. Después de la fusión — ejecuta `regen.sh`.

## Dos productos de producción construidos en esta biblioteca

**iDryer Storage Link** — ESP32-C3 con una tira LED WS2812B y un sensor de temperatura/humedad SHT31.

**iHeater Link** — ESP32-C3 con salida RMT al calentador iHeater, con integraciones para Bambu Lab, Klipper/Moonraker y Home Assistant.

Ambos productos incluyen idryer-core a través de PlatformIO `lib_deps` e implementan solo su lógica específica del producto.

## Qué sigue

- [01-wifi.md](01-wifi.md) — conecta el ESP32 a WiFi usando Improv-Serial.
- [../../../README.md](../../../README.md) — descripción general de la biblioteca y referencia de generación de código.
