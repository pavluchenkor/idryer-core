# Detailed setup

The short path is [Run in 5 minutes](01-five-minutes.md). This page covers the environment, build flags, logs and development mode.

## The core in the project

The core lives in `lib/idryer-core` of the PlatformIO project: as a copy, a git submodule or a symbolic link to a shared clone. Its `library.json` brings the dependencies: MQTT, ArduinoJson, WebSockets, Improv. The project's `lib_deps` holds only the libraries of your sensors.

Boards of the products built on the core: ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP is the ESP8266 transport from espMqttClient dependencies: it does not build on ESP32.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Board without USB-UART (ESP32-C3 SuperMini): Serial over USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Flag | Why |
|---|---|
| `IDRYER_API_BASE` | portal API address: activation, pairing |
| `MQTT_BROKER`, `MQTT_PORT` | the portal broker |
| `MQTT_USE_TLS=1` | secure connection to the broker |
| `lib_ignore = ESPAsyncTCP` | ESP8266 transport from the MQTT client dependencies: it does not build on ESP32 |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial over USB on boards without USB-UART |

String macros need quotes both outside and inside.

## Publishing periods

The `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs` fields; zero means the contract value:

| What | Working | Idle |
|---|---|---|
| telemetry | 30 s | 60 s |
| status | immediately on a change of mode, setpoint or time; reconciliation every 60 s | reconciliation every 5 min |

"Idle" means no unit is in an active mode.

## Logs

```bash
pio device monitor -b 115200
```

While the device has no network, the log is silent: the port is busy with Improv. Logs turn on with the `[BOOT] WiFi ok, logs enabled` line. After that the `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` commands work: see [Linking to an account](02-claim.md).

## Development mode: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

With the flag:

- logs go to the port right after power-on;
- Improv and the `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` commands are off: your code reads the incoming lines of the port;
- the network comes from the app (ESPTouch) or from code: `seedWifiCredentialsIfEmpty()` before `begin()`, see [Wi-Fi](01-wifi.md).

Release firmware is built without the flag.

## Next

- [Core examples](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [How to add a new product](../09-add-product/01-add-new-product.md).
- [Device card: the card manifest](../09-add-product/02-add-widget.md).
