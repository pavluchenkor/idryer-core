---
title: "Arrancar un dispositivo con idryer-core en 5 minutos"
description: "El primer dispositivo con idryer-core: proyecto PlatformIO, firmware mínimo, Wi-Fi y vinculación con la cuenta en la aplicación iDryer."
---

# Arrancar un dispositivo con idryer-core en 5 minutos

Después de esta página el ESP32 está en la red, vinculado a tu cuenta y visible en el portal [portal.idryer.org](https://portal.idryer.org/) y en la aplicación iDryer. Necesitas: una placa ESP32-C3 (DevKit, Super Mini o compatible), un cable USB, PlatformIO en VS Code, un teléfono con la aplicación iDryer, una red Wi-Fi de 2,4 GHz.

## 1. Proyecto PlatformIO

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← copia, submódulo git o enlace simbólico
└── src/
    └── main.cpp
```

`platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP es el transporte de ESP8266 de las dependencias de espMqttClient: no compila en ESP32.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Placa sin USB-UART (ESP32-C3 SuperMini): Serial por USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

Las bibliotecas del núcleo (MQTT, ArduinoJson, WebSockets, Improv) llegan solas desde el `library.json` del núcleo. Una placa con USB-UART no necesita las dos últimas opciones; ajusta `board` a tu placa.

## 2. Código

Copia el ejemplo [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) en `src/main.cpp`:

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // tu propio dispositivo
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Periodo de parpadeo según el estado; 0: no parpadear.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Desvinculación: el núcleo borra el secreto y vuelve a esperar la vinculación.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // El núcleo publica solo los campos de telemetría, cada 30 s (cada 60 s en reposo).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

El firmware no tiene ni la contraseña de la red ni datos de la cuenta: el dispositivo recibe la red y la vinculación desde la aplicación.

## 3. Grabar y abrir el log

```bash
pio run -t upload
pio device monitor -b 115200
```

Mientras el dispositivo no tiene Wi-Fi, el log calla: el núcleo reserva el puerto para Improv. El LED parpadea rápido: el dispositivo espera la red y la vinculación.

## 4. Wi-Fi y vinculación en la aplicación

1. Conecta el teléfono a la red Wi-Fi en la que funcionará el dispositivo e inicia sesión en la aplicación iDryer con tu cuenta del portal.
2. En la pantalla de inicio, toca **Conectar un dispositivo nuevo**: se abre el paso **Wi-Fi**.
3. Comprueba el nombre de la red, escribe la contraseña y toca **Conectar dispositivo**. La aplicación envía la configuración durante hasta 90 segundos; cuando el dispositivo se une, aparece **Dispositivo conectado**. Toca **Siguiente**.
4. En el paso **Vinculación**, toca **Vincular**. La aplicación encuentra el dispositivo en la red, obtiene del portal un token de vinculación de un solo uso y se lo entrega al dispositivo.
5. Tras **Dispositivo vinculado**, el dispositivo aparece en la lista de dispositivos del portal y de la aplicación.

El log después de unirse a la red:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Después de la vinculación:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Comprobación

- el dispositivo está en línea en el portal y en la aplicación;
- la tarjeta muestra la temperatura del chip ESP32;
- el LED parpadea una vez por segundo. Encendido o apagado fijo significa que no hay conexión con el portal.

## Si no funcionó

- la aplicación no vio conectarse el dispositivo: revisa la contraseña y que la red sea de 2,4 GHz; con una contraseña errónea el dispositivo vuelve a esperar la configuración. Otras formas de pasar la red: [Wi-Fi](01-wifi.md);
- el paso **Vinculación** no encontró el dispositivo: el teléfono y el dispositivo deben estar en la misma red; las redes de invitados suelen bloquear el descubrimiento de dispositivos. Más: [Vinculación con la cuenta](02-claim.md);
- compilación y opciones: [Configuración detallada](99-detailed-setup.md).

## Siguiente

- [Telemetría](03-telemetry.md): un sensor y un valor propio en la tarjeta.
- [Ejemplos del núcleo](https://github.com/pavluchenkor/idryer-core/tree/main/examples): acciones de tarjeta, sensores y controles propios.
