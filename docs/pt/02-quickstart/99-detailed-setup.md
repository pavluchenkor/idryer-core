# Configuração detalhada

O caminho curto é [Arrancar em 5 minutos](01-five-minutes.md). Esta página trata do ambiente, das opções de compilação, dos logs e do modo de desenvolvimento.

## O núcleo no projeto

O núcleo fica em `lib/idryer-core` do projeto PlatformIO: como cópia, submódulo git ou ligação simbólica para um clone partilhado. O seu `library.json` traz as dependências: MQTT, ArduinoJson, WebSockets, Improv. O `lib_deps` do projeto tem só as bibliotecas dos seus sensores.

Placas dos produtos sobre o núcleo: ESP32-C3 (DevKit, Super Mini), ESP32-S3 (XIAO ESP32-S3, Waveshare ESP32-S3 Zero).

## `platformio.ini`

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP é o transporte ESP8266 das dependências do espMqttClient: não compila no ESP32.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Placa sem USB-UART (ESP32-C3 SuperMini): Serial por USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Flag | Para quê |
|---|---|
| `IDRYER_API_BASE` | endereço da API do portal: ativação, associação |
| `MQTT_BROKER`, `MQTT_PORT` | o broker do portal |
| `MQTT_USE_TLS=1` | ligação segura ao broker |
| `lib_ignore = ESPAsyncTCP` | transporte ESP8266 das dependências do cliente MQTT: não compila no ESP32 |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial por USB em placas sem USB-UART |

As macros de texto precisam de aspas por fora e por dentro.

## Períodos de publicação

Os campos `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs`; zero significa o valor do contrato:

| O quê | Em funcionamento | Em repouso |
|---|---|---|
| telemetria | 30 s | 60 s |
| estado | logo que muda o modo, o setpoint ou o tempo; conciliação a cada 60 s | conciliação a cada 5 min |

«Em repouso» significa que nenhuma unidade está num modo ativo.

## Logs

```bash
pio device monitor -b 115200
```

Enquanto o dispositivo não tem rede, o log fica calado: a porta está ocupada pelo Improv. Os logs ativam-se com a linha `[BOOT] WiFi ok, logs enabled`. Depois funcionam os comandos `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN`: ver [Associação à conta](02-claim.md).

## Modo de desenvolvimento: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Com a flag:

- os logs vão para a porta logo após ligar;
- o Improv e os comandos `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` ficam desativados: o seu código lê as linhas que chegam à porta;
- a rede chega da aplicação (ESPTouch) ou do código: `seedWifiCredentialsIfEmpty()` antes de `begin()`, ver [Wi-Fi](01-wifi.md).

O firmware publicado compila-se sem a flag.

## A seguir

- [Exemplos do núcleo](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Como adicionar um produto novo](../09-add-product/01-add-new-product.md).
- [Cartão do dispositivo: o card manifest](../09-add-product/02-add-widget.md).
