# Configuração detalhada

O caminho curto é [Rodar em 5 minutos](01-five-minutes.md). Esta página trata do ambiente, das flags de build, dos logs e do modo de desenvolvimento.

## O núcleo no projeto

O núcleo fica em `lib/idryer-core` do projeto PlatformIO: como cópia, submódulo git ou link simbólico para um clone compartilhado. O `library.json` dele traz as dependências: MQTT, ArduinoJson, WebSockets, Improv. O `lib_deps` do projeto tem só as bibliotecas dos seus sensores.

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
    ; Placa sem USB-UART (ESP32-C3 SuperMini): Serial pela USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

| Flag | Para quê |
|---|---|
| `IDRYER_API_BASE` | endereço da API do portal: ativação, vinculação |
| `MQTT_BROKER`, `MQTT_PORT` | o broker do portal |
| `MQTT_USE_TLS=1` | conexão segura com o broker |
| `lib_ignore = ESPAsyncTCP` | transporte ESP8266 das dependências do cliente MQTT: não compila no ESP32 |
| `ARDUINO_USB_MODE`, `ARDUINO_USB_CDC_ON_BOOT` | Serial pela USB em placas sem USB-UART |

As macros de texto precisam de aspas por fora e por dentro.

## Períodos de publicação

Os campos `Config.telemetryPeriodMs`, `telemetryPeriodIdleMs`, `statusPeriodMs`, `statusPeriodIdleMs`; zero significa o valor do contrato:

| O quê | Em funcionamento | Em repouso |
|---|---|---|
| telemetria | 30 s | 60 s |
| status | na hora, quando muda o modo, o setpoint ou o tempo; conferência a cada 60 s | conferência a cada 5 min |

"Em repouso" significa que nenhuma unidade está em modo ativo.

## Logs

```bash
pio device monitor -b 115200
```

Enquanto o dispositivo não tem rede, o log fica em silêncio: a porta está ocupada pelo Improv. Os logs ligam com a linha `[BOOT] WiFi ok, logs enabled`. Depois disso funcionam os comandos `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN`: veja [Vinculação à conta](02-claim.md).

## Modo de desenvolvimento: `IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

Com a flag:

- os logs vão para a porta logo depois de ligar;
- o Improv e os comandos `STATUS`, `WIPE_IDENTITY`, `PAIR_TOKEN` ficam desligados: seu código lê as linhas que chegam na porta;
- a rede vem do app (ESPTouch) ou do código: `seedWifiCredentialsIfEmpty()` antes de `begin()`, veja [Wi-Fi](01-wifi.md).

O firmware publicado é compilado sem a flag.

## Próximo passo

- [Exemplos do núcleo](https://github.com/pavluchenkor/idryer-core/tree/main/examples).
- [Como adicionar um produto novo](../09-add-product/01-add-new-product.md).
- [Card do dispositivo: o card manifest](../09-add-product/02-add-widget.md).
