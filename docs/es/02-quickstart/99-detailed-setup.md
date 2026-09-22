# Configuración detallada

La vía corta es [Arrancar en 5 minutos](01-five-minutes.md). Esta página cubre el entorno, las opciones de compilación, los logs y el modo de desarrollo.

## El núcleo en el proyecto

El núcleo está en `lib/idryer-core` del proyecto PlatformIO: como copia, submódulo git o enlace simbólico a un clon compartido. Su `library.json` trae las dependencias: MQTT, ArduinoJson, WebSockets, Improv. El `lib_deps` del proyecto solo lleva las bibliotecas de tus sensores.

Placas de los productos sobre el núcleo: ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

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

| Opción | Para qué |
|---|---|
| `IDRYER_API_BASE` | dirección de la API del portal: activación, vinculación |
| `MQTT_BROKER`, `MQTT_PORT` | el broker del portal |
| `MQTT_USE_TLS=1` | conexión segura con el broker |
| `lib_ignore = ESPAsyncTCP` | transporte de ESP8266 de las dependencias del cliente MQTT: no compila en ESP32 |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial por USB en placas sin USB-UART |

Las macros de cadena necesitan comillas por fuera y por dentro.

## Periodos de publicación

Los campos `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs`; cero significa el valor del contrato:

| Qué | En funcionamiento | En reposo |
|---|---|---|
| telemetría | 30 s | 60 s |
| estado | al instante al cambiar modo, consigna o tiempo; conciliación cada 60 s | conciliación cada 5 min |

«En reposo» significa que ninguna unidad está en un modo activo.

## Logs

```bash
pio device monitor -b 115200
```

Mientras el dispositivo no tiene red, el log calla: el puerto está ocupado por Improv. Los logs se activan con la línea `[BOOT] WiFi ok, logs enabled`. Después funcionan los comandos `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN`: ver [Vinculación con la cuenta](02-claim.md).

## Modo de desarrollo: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Con la opción:

- los logs van al puerto nada más encender;
- Improv y los comandos `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` están desactivados: tu código lee las líneas entrantes del puerto;
- la red llega desde la aplicación (ESPTouch) o desde el código: `seedWifiCredentialsIfEmpty()` antes de `begin()`, ver [Wi-Fi](01-wifi.md).

El firmware publicado se compila sin la opción.

## Siguiente

- [Ejemplos del núcleo](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Cómo añadir un producto nuevo](../09-add-product/01-add-new-product.md).
- [Tarjeta del dispositivo: el card manifest](../09-add-product/02-add-widget.md).
