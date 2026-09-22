---
title: "Rodar um dispositivo no idryer-core em 5 minutos"
description: "O primeiro dispositivo no idryer-core: projeto PlatformIO, firmware mínimo, Wi-Fi e vinculação à conta no app iDryer."
---

# Rodar um dispositivo no idryer-core em 5 minutos

Depois desta página, o ESP32 está na rede, vinculado à sua conta e visível no portal [portal.idryer.org](https://portal.idryer.org/) e no app iDryer. Você vai precisar de: uma placa ESP32-C3 (DevKit, Super Mini ou compatível), um cabo USB, PlatformIO no VS Code, um celular com o app iDryer, uma rede Wi-Fi de 2,4 GHz.

## 1. Projeto PlatformIO

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← cópia, submódulo git ou link simbólico
└── src/
    └── main.cpp
```

`platformio.ini`:

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

As bibliotecas do núcleo (MQTT, ArduinoJson, WebSockets, Improv) chegam sozinhas pelo `library.json` do núcleo. Uma placa com USB-UART não precisa das duas últimas flags; ajuste `board` para a sua placa.

## 2. Código

Copie o exemplo [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) para `src/main.cpp`:

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // seu próprio dispositivo
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Período de piscar conforme o estado; 0: não piscar.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Desvinculação: o núcleo apaga o segredo e volta a esperar a vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // O núcleo publica sozinho os campos de telemetria, a cada 30 s (a cada 60 s em repouso).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

O firmware não tem a senha da rede nem dados da conta: o dispositivo recebe a rede e a vinculação pelo app.

## 3. Gravar e abrir o log

```bash
pio run -t upload
pio device monitor -b 115200
```

Enquanto o dispositivo não tem Wi-Fi, o log fica em silêncio: o núcleo segura a porta para o Improv. O LED pisca rápido: o dispositivo espera a rede e a vinculação.

## 4. Wi-Fi e vinculação no app

1. Conecte o celular à rede Wi-Fi em que o dispositivo vai funcionar e entre no app iDryer com a sua conta do portal.
2. Na tela inicial, toque em **Conectar novo dispositivo**: abre o passo **Wi-Fi**.
3. Confira o nome da rede, digite a senha e toque em **Conectar dispositivo**. O app envia a configuração por até 90 segundos; quando o dispositivo entra, aparece **Dispositivo conectado**. Toque em **Avançar**.
4. No passo **Vinculação**, toque em **Vincular**. O app encontra o dispositivo na rede, pega no portal um token de vinculação de uso único e entrega ao dispositivo.
5. Depois de **Dispositivo vinculado**, ele aparece na lista de dispositivos do portal e do app.

O log depois de entrar na rede:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Depois da vinculação:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Verificação

- o dispositivo está online no portal e no app;
- o card mostra a temperatura do chip ESP32;
- o LED pisca uma vez por segundo. Aceso ou apagado direto: sem conexão com o portal.

## Se não deu certo

- o app não viu o dispositivo conectar: confira a senha e se a rede é de 2,4 GHz; com a senha errada, o dispositivo volta a esperar a configuração. Outros jeitos de passar a rede: [Wi-Fi](01-wifi.md);
- o passo **Vinculação** não achou o dispositivo: o celular e o dispositivo precisam estar na mesma rede; redes de convidados costumam bloquear a descoberta de dispositivos. Mais: [Vinculação à conta](02-claim.md);
- build e flags: [Configuração detalhada](99-detailed-setup.md).

## Próximo passo

- [Telemetria](03-telemetry.md): um sensor e um valor seu no card.
- [Exemplos do núcleo](https://github.com/pavluchenkor/idryer-core/tree/main/examples): ações do card, sensores e controles seus.
