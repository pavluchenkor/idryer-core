# ¿Qué es idryer-core

Si está construyendo un dispositivo ESP32 para la nube iDryer, esta biblioteca maneja el aprovisionamiento de WiFi (Improv), el protocolo de reclamación, la sesión MQTT (TLS, reconexión, sincronización de hora), la publicación periódica de telemetría/estado y el enrutamiento de comandos entrantes. Aproximadamente 500 líneas de código repetitivo se reducen a `link.begin(); link.loop();`.

## Ejemplo mínimo

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

## Lo que hace la biblioteca

- Conexión WiFi y mantenimiento vivo; aprovisionamiento de Improv sobre Web Serial para la configuración inicial.
- Protocolo de reclamación: registro de dispositivo en el backend, reclamación de cuenta mediante PIN.
- Sesión MQTT con el corredor iDryer: TLS, sesión persistente, reconexión automática, sincronización de hora NTP.
- Publicación periódica de telemetría (`Telemetry`) y estado (`Status`) en un temporizador.
- Enrutamiento de comandos entrantes (`commands/invoke`, `commands/set`, `commands/ping`) al controlador de productos.
- Servidor WebSocket local: un cliente LAN ve el mismo flujo que la nube.
- Persistencia NVS: credenciales WiFi, token de dispositivo, configuración de menú a través de reinicios.

## Lo que la biblioteca no hace

- No gestiona hardware del producto: ventiladores, calentadores, tiras LED, sensores.
- No contiene lógica comercial para secado, almacenamiento o iluminación.
- No conoce parámetros de menú específicos del producto — solo los transporta.
- No publica telemetría sin datos del producto: usted rellena `link.telemetry.*` en `loop()`.

## Dónde ir a continuación

- [Comenzar en 5 minutos](../02-quickstart/01-five-minutes.md)
- [API completa: iDryer::Link](../03-public-api/01-link-api-reference.md)
