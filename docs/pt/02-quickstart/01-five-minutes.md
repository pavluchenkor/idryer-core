---
title: "Arrancar um dispositivo no idryer-core em 5 minutos"
description: "O primeiro dispositivo no idryer-core: projeto PlatformIO, firmware mínimo, Wi-Fi e associação à conta na aplicação iDryer."
---

# Arrancar um dispositivo no idryer-core em 5 minutos

Depois desta página, o ESP32 está na rede, associado à sua conta e visível no portal [portal.idryer.org](https://portal.idryer.org/) e na aplicação iDryer. Precisa de: uma placa ESP32-C3 (DevKit, Super Mini ou compatível), um cabo USB, PlatformIO no VS Code, um telemóvel com a aplicação iDryer, uma rede Wi-Fi de 2,4 GHz.

## 1. Projeto PlatformIO

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← cópia, submódulo git ou ligação simbólica
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
    ; Placa sem USB-UART (ESP32-C3 SuperMini): Serial por USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

As bibliotecas do núcleo (MQTT, ArduinoJson, WebSockets, Improv) chegam sozinhas pelo `library.json` do núcleo. Uma placa com USB-UART não precisa das duas últimas flags; ajuste `board` à sua placa.

## 2. Código

Copie o exemplo [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) para `src/main.cpp`:

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // o seu próprio dispositivo
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Período de intermitência conforme o estado; 0: não piscar.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Desassociação: o núcleo apaga o segredo e volta a esperar pela associação.
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

O firmware não tem a palavra-passe da rede nem dados da conta: o dispositivo recebe a rede e a associação da aplicação.

## 3. Gravar e abrir o log

```bash
pio run -t upload
pio device monitor -b 115200
```

Enquanto o dispositivo não tem Wi-Fi, o log fica calado: o núcleo reserva a porta para o Improv. O LED pisca depressa: o dispositivo espera pela rede e pela associação.

## 4. Wi-Fi e associação na aplicação

1. Ligue o telemóvel à rede Wi-Fi em que o dispositivo vai funcionar e inicie sessão na aplicação iDryer com a sua conta do portal.
2. No ecrã inicial, toque em **Ligar novo dispositivo**: abre-se o passo **Wi-Fi**.
3. Confirme o nome da rede, escreva a palavra-passe e toque em **Ligar dispositivo**. A aplicação envia a configuração durante até 90 segundos; quando o dispositivo entra, aparece **Dispositivo ligado**. Toque em **Seguinte**.
4. No passo **Vinculação**, toque em **Emparelhar**. A aplicação encontra o dispositivo na rede, obtém do portal um token de associação de uso único e entrega-o ao dispositivo.
5. Depois de **Dispositivo emparelhado**, o dispositivo aparece na lista de dispositivos do portal e da aplicação.

O log depois de entrar na rede:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Depois da associação:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Verificação

- o dispositivo está online no portal e na aplicação;
- o cartão mostra a temperatura do chip ESP32;
- o LED pisca uma vez por segundo. Aceso ou apagado fixo: sem ligação ao portal.

## Se não funcionou

- a aplicação não viu o dispositivo ligar-se: verifique a palavra-passe e se a rede é de 2,4 GHz; com a palavra-passe errada, o dispositivo volta a esperar pela configuração. Outras formas de passar a rede: [Wi-Fi](01-wifi.md);
- o passo **Vinculação** não encontrou o dispositivo: o telemóvel e o dispositivo têm de estar na mesma rede; as redes de convidados bloqueiam muitas vezes a descoberta de dispositivos. Mais: [Associação à conta](02-claim.md);
- compilação e flags: [Configuração detalhada](99-detailed-setup.md).

## A seguir

- [Telemetria](03-telemetry.md): um sensor e um valor seu no cartão.
- [Exemplos do núcleo](https://github.com/pavluchenkor/idryer-core/tree/main/examples): ações do cartão, sensores e controlos seus.
