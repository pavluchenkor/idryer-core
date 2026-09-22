---
title: "Como adicionar um novo produto baseado em idryer-core"
description: "Checklist para um novo dispositivo iDryer sobre a fachada iDryer::Link: projeto, firmware mínimo, Wi-Fi e vinculação no app, telemetria, menu de configurações, cartão do dispositivo, contrato."
---

# Como adicionar um novo produto baseado em idryer-core

Use este guia quando for criar um novo produto sobre o `idryer-core`: um secador de filamento, um bloco de aquecimento, uma iluminação, um sensor ou outro módulo. Ele mostra o que o core faz por você e o que o código do produto precisa adicionar.

Um exemplo completo que compila é o gabinete de armazenamento aquecido da documentação Build-Your-Own-iDryer (`example/09-cabinet`): ele passa por tudo o que está nesta página.

---

## Sobre o que um produto é construído

Um produto conversa com o core por um único objeto — a fachada `iDryer::Link` (`<iDryer.h>`). Dentro de `s_link.begin()` e `s_link.loop()`, o core:

- recebe a rede Wi-Fi do app iDryer pelo ar (ESPTouch) ou do instalador web por USB (Improv) e mantém a conexão;
- vincula o dispositivo a uma conta: espera um token de vinculação de uso único — do app pela rede local ou pela porta serial (`PAIR_TOKEN:<token>`) — e troca no portal por um segredo permanente;
- conecta ao MQTT e publica `telemetry` e `status` nos períodos de `Config`;
- se anuncia na rede local (mDNS `_idryer._tcp`) e aceita comandos do app por WebSocket;
- publica o card manifest do cartão do dispositivo.

As classes de nível mais baixo (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` e outras) são internas do core: o token de vinculação só chega à parte de nuvem dentro de `iDryer::Link`. Construa o produto sobre a fachada.

---

## 1. Projeto

O `idryer-core` vai em `lib/idryer-core/` (cópia ou link simbólico); o PlatformIO pega as bibliotecas do core do `library.json` dele. `platformio.ini` mínimo:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; transporte do ESP8266 que vem das dependências do espMqttClient: não compila no ESP32
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Sem `lib_ignore = ESPAsyncTCP` o build falha em `ESPAsyncTCP.cpp`; sem `MQTT_BROKER` e `MQTT_PORT` o core não compila.

---

## 2. Firmware mínimo

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // não é produto iDryer: o cartão vem do manifesto
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
    // Dispositivo desvinculado no portal: apagar o segredo e aguardar nova vinculação.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // suas funções de sensores
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` sobe o Wi-Fi, a vinculação, o MQTT e o acesso local; `s_link.loop()` precisa rodar o tempo todo, sem `delay()`. O comando `revoke` chega do portal quando o dispositivo é desvinculado da conta: `handleRevoke()` apaga o segredo e o dispositivo espera um novo token de vinculação.

---

## 3. Wi-Fi e vinculação — nada no código

O firmware não tem a senha da rede nem dados da conta. O usuário conecta o dispositivo no app iDryer: **Conectar novo dispositivo** → passo **Wi-Fi** (o app envia a rede por ESPTouch) → passo **Vinculação** (o app encontra o dispositivo por mDNS, pega no portal um token de uso único e entrega ao dispositivo; o próprio dispositivo ativa o token no portal). Enquanto o Wi-Fi não sobe, a porta serial fica em silêncio: o core a reserva para o instalador web (Improv).

Passo a passo e com o log esperado — Build-Your-Own-iDryer, capítulo “Início do firmware no core”.

---

## 4. Dados: telemetria e status

- As flags `has*` em `Config` definem quais campos do vocabulário vão para a telemetria e quais células aparecem no cartão.
- Grave os valores em `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) e `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); o core os publica nos períodos de `Config`, e `s_link.publishStatusNow()` envia o status na hora.
- Um campo próprio — pelo `s_link.onTelemetryPublish()`, no cartão — pelo `s_link.card().sensor()`; veja [Cartão do dispositivo](02-add-widget.md).

---

## 5. Configurações: o menu

As configurações são descritas em `src/menu/menu.yaml`; o gerador `menu_gen.py` produz o código C++, o armazenamento na NVS e o JSON do menu ([Menu como Protocolo](../08-contracts/02-menu-as-protocol.md)). O código do produto:

- carrega o menu antes de `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- publica com `menu_buildFullJson()` e `s_link.devicePublisher()->publishConfigRaw()` — ao ficar online e no comando `get_config`;
- aplica o comando `set` com `menu_apply_by_bind()` (valor, NVS e cache de uma vez) e publica o menu de novo.

O código completo — Build-Your-Own-iDryer, capítulo “Menu a partir de YAML”.

---

## 6. Cartão do dispositivo

O que o cartão mostra e quais operações ele dispara é declarado com `s_link.card()` — [Cartão do dispositivo: o card manifest](02-add-widget.md).

---

## 7. Contrato

Quando você adiciona topics novos ou muda payloads:

1. atualize `contracts/mqtt_contract.yaml`;
2. rode `contracts/regen.sh` e faça commit dos arquivos gerados.

---

## Dispositivos de dois chips

Para um ESP32 que trabalha com um controlador separado (por exemplo, RP2040) via UART, o core tem a ponte UART `idryer_uart.h`; a referência que funciona é o firmware `idryer-link`.
