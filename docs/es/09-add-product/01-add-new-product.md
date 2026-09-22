---
title: "Cómo añadir un nuevo producto basado en idryer-core"
description: "Lista de comprobación para un nuevo dispositivo iDryer sobre la fachada iDryer::Link: proyecto, firmware mínimo, Wi-Fi y vinculación en la aplicación, telemetría, menú de ajustes, tarjeta del dispositivo, contrato."
---

# Cómo añadir un nuevo producto basado en idryer-core

Usa esta guía cuando crees un producto nuevo sobre `idryer-core`: un secador de filamento, un bloque calefactor, una iluminación, un sensor u otro módulo. Muestra qué hace el core por ti y qué debe añadir el código del producto.

Un ejemplo completo que compila es el armario de almacenamiento calefactado de la documentación Build-Your-Own-iDryer (`example/09-cabinet`): recorre todo lo que hay en esta página.

---

## Sobre qué se construye un producto

Un producto habla con el core a través de un solo objeto: la fachada `iDryer::Link` (`<iDryer.h>`). Dentro de `s_link.begin()` y `s_link.loop()` el core:

- obtiene la red Wi-Fi desde la aplicación iDryer por el aire (ESPTouch) o desde el instalador web por USB (Improv) y mantiene la conexión;
- vincula el dispositivo a una cuenta: espera un token de vinculación de un solo uso —desde la aplicación por la red local o por el puerto serie (`PAIR_TOKEN:<token>`)— y lo canjea en el portal por un secreto permanente;
- se conecta a MQTT y publica `telemetry` y `status` con los periodos de `Config`;
- se anuncia en la red local (mDNS `_idryer._tcp`) y acepta comandos de la aplicación por WebSocket;
- publica el card manifest de la tarjeta del dispositivo.

Las clases de nivel inferior (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` y otras) son partes internas del core: el token de vinculación llega a la parte de nube solo dentro de `iDryer::Link`. Construye el producto sobre la fachada.

---

## 1. Proyecto

`idryer-core` va en `lib/idryer-core/` (una copia o un enlace simbólico); PlatformIO toma las bibliotecas del core de su `library.json`. `platformio.ini` mínimo:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; transporte de ESP8266 de las dependencias de espMqttClient: no compila en ESP32
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Sin `lib_ignore = ESPAsyncTCP` la compilación falla en `ESPAsyncTCP.cpp`; sin `MQTT_BROKER` y `MQTT_PORT` el core no compila.

---

## 2. Firmware mínimo

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // no es un producto iDryer: la tarjeta sale del manifiesto
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
    // El dispositivo se desvinculó en el portal: borrar el secreto y esperar una nueva vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // tus funciones de sensores
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` arranca el Wi-Fi, la vinculación, MQTT y el acceso local; `s_link.loop()` tiene que ejecutarse siempre, sin `delay()`. El comando `revoke` llega desde el portal cuando el dispositivo se desvincula de la cuenta: `handleRevoke()` borra el secreto y el dispositivo espera un nuevo token de vinculación.

---

## 3. Wi-Fi y vinculación: nada en el código

El firmware no contiene ni la contraseña de la red ni datos de la cuenta. El usuario conecta el dispositivo en la aplicación iDryer: **Conectar un dispositivo nuevo** → paso **Wi-Fi** (la aplicación envía la red por ESPTouch) → paso **Vinculación** (la aplicación encuentra el dispositivo por mDNS, obtiene del portal un token de un solo uso y se lo entrega; el dispositivo activa el token en el portal por sí mismo). Mientras no hay Wi-Fi, el puerto serie está en silencio: el core lo reserva para el instalador web (Improv).

Paso a paso y con el log esperado: Build-Your-Own-iDryer, capítulo «Inicio del firmware en el core».

---

## 4. Datos: telemetría y estado

- Los flags `has*` de `Config` definen qué campos del vocabulario van a la telemetría y qué celdas aparecen en la tarjeta.
- Escribe los valores en `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) y `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); el core los publica con los periodos de `Config` y `s_link.publishStatusNow()` envía el estado al momento.
- Un campo propio: con `s_link.onTelemetryPublish()`; en la tarjeta, con `s_link.card().sensor()`; ver [Tarjeta del dispositivo](02-add-widget.md).

---

## 5. Ajustes: el menú

Los ajustes se describen en `src/menu/menu.yaml`; el generador `menu_gen.py` produce código C++, el almacenamiento en NVS y el JSON del menú ([El menú como protocolo](../08-contracts/02-menu-as-protocol.md)). El código del producto:

- carga el menú antes de `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- lo publica con `menu_buildFullJson()` y `s_link.devicePublisher()->publishConfigRaw()`: al pasar a estar en línea y con el comando `get_config`;
- aplica el comando `set` con `menu_apply_by_bind()` (valor, NVS y caché a la vez) y vuelve a publicar el menú.

El código completo: Build-Your-Own-iDryer, capítulo «Menú desde YAML».

---

## 6. Tarjeta del dispositivo

Lo que muestra la tarjeta y qué operaciones lanza se declara con `s_link.card()`: [Tarjeta del dispositivo: el card manifest](02-add-widget.md).

---

## 7. Contrato

Cuando añadas topics nuevos o cambies payloads:

1. actualiza `contracts/mqtt_contract.yaml`;
2. ejecuta `contracts/regen.sh` y haz commit de los archivos generados.

---

## Dispositivos de dos chips

Para un ESP32 que trabaja con un controlador aparte (por ejemplo, RP2040) por UART, el core tiene el puente UART `idryer_uart.h`; la referencia que funciona es el firmware `idryer-link`.
