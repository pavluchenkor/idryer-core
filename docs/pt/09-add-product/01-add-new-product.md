---
title: "Como adicionar um novo produto baseado em idryer-core"
description: "Checklist para um novo dispositivo iDryer sobre a fachada iDryer::Link: projeto, firmware mínimo, Wi-Fi e vinculação na aplicação, telemetria, menu de definições, cartão do dispositivo, contrato."
---

# Como adicionar um novo produto baseado em idryer-core

Use este guia quando criar um novo produto sobre `idryer-core`: um secador de filamento, um bloco de aquecimento, uma iluminação, um sensor ou outro módulo. Mostra o que o core faz por si e o que o código do produto tem de acrescentar.

Um exemplo completo que compila é o armário de armazenamento aquecido da documentação Build-Your-Own-iDryer (`example/09-cabinet`): percorre tudo o que está nesta página.

---

## Em que assenta um produto

Um produto fala com o core através de um único objeto — a fachada `iDryer::Link` (`<iDryer.h>`). Dentro de `s_link.begin()` e `s_link.loop()`, o core:

- recebe a rede Wi-Fi da aplicação iDryer pelo ar (ESPTouch) ou do instalador web por USB (Improv) e mantém a ligação;
- associa o dispositivo a uma conta: espera um token de vinculação de uso único — da aplicação pela rede local ou pela porta série (`PAIR_TOKEN:<token>`) — e troca-o no portal por um segredo permanente;
- liga-se ao MQTT e publica `telemetry` e `status` com os períodos de `Config`;
- anuncia-se na rede local (mDNS `_idryer._tcp`) e aceita comandos da aplicação por WebSocket;
- publica o card manifest do cartão do dispositivo.

As classes de nível inferior (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` e outras) são internas do core: o token de vinculação só chega à parte de nuvem dentro de `iDryer::Link`. Construa o produto sobre a fachada.

---

## 1. Projeto

`idryer-core` vai para `lib/idryer-core/` (cópia ou ligação simbólica); o PlatformIO obtém as bibliotecas do core a partir do seu `library.json`. `platformio.ini` mínimo:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; transporte ESP8266 das dependências do espMqttClient: não compila no ESP32
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Sem `lib_ignore = ESPAsyncTCP` a compilação falha em `ESPAsyncTCP.cpp`; sem `MQTT_BROKER` e `MQTT_PORT` o core não compila.

---

## 2. Firmware mínimo

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // não é um produto iDryer: o cartão vem do manifesto
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
    // Dispositivo desassociado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // as suas funções de sensores
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` arranca o Wi-Fi, a vinculação, o MQTT e o acesso local; `s_link.loop()` tem de correr sempre, sem `delay()`. O comando `revoke` chega do portal quando o dispositivo é desassociado da conta: `handleRevoke()` apaga o segredo e o dispositivo espera um novo token de vinculação.

---

## 3. Wi-Fi e vinculação — nada no código

O firmware não contém a palavra-passe da rede nem dados da conta. O utilizador liga o dispositivo na aplicação iDryer: **Ligar novo dispositivo** → passo **Wi-Fi** (a aplicação envia a rede por ESPTouch) → passo **Vinculação** (a aplicação encontra o dispositivo por mDNS, obtém do portal um token de uso único e entrega-o ao dispositivo; o próprio dispositivo ativa o token no portal). Enquanto o Wi-Fi não está ativo, a porta série fica em silêncio: o core reserva-a para o instalador web (Improv).

Passo a passo e com o log esperado — Build-Your-Own-iDryer, capítulo «Arranque do firmware no core».

---

## 4. Dados: telemetria e estado

- As flags `has*` em `Config` definem que campos do vocabulário vão para a telemetria e que células aparecem no cartão.
- Escreva os valores em `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) e `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); o core publica-os com os períodos de `Config` e `s_link.publishStatusNow()` envia o estado de imediato.
- Um campo próprio — através de `s_link.onTelemetryPublish()`, no cartão — através de `s_link.card().sensor()`; ver [Cartão do dispositivo](02-add-widget.md).

---

## 5. Definições: o menu

As definições descrevem-se em `src/menu/menu.yaml`; o gerador `menu_gen.py` produz o código C++, o armazenamento em NVS e o JSON do menu ([Menu como Protocolo](../08-contracts/02-menu-as-protocol.md)). O código do produto:

- carrega o menu antes de `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- publica-o com `menu_buildFullJson()` e `s_link.devicePublisher()->publishConfigRaw()` — ao ficar online e com o comando `get_config`;
- aplica o comando `set` com `menu_apply_by_bind()` (valor, NVS e cache de uma vez) e publica o menu de novo.

O código completo — Build-Your-Own-iDryer, capítulo «Menu a partir de YAML».

---

## 6. Cartão do dispositivo

O que o cartão mostra e que operações lança declara-se com `s_link.card()` — [Cartão do dispositivo: o card manifest](02-add-widget.md).

---

## 7. Contrato

Quando adicionar topics novos ou alterar payloads:

1. atualize `contracts/mqtt_contract.yaml`;
2. execute `contracts/regen.sh` e faça commit dos ficheiros gerados.

---

## Dispositivos de dois chips

Para um ESP32 que trabalha com um controlador separado (por exemplo, RP2040) por UART, o core tem a ponte UART `idryer_uart.h`; a referência funcional é o firmware `idryer-link`.
