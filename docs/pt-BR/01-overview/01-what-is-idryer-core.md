# O que é idryer-core

Se você está criando um dispositivo ESP32 para a nuvem iDryer, esta biblioteca trata do provisionamento WiFi (Improv), protocolo de reivindicação, sessão MQTT (TLS, reconexão, sincronização de hora), publicação periódica de telemetria/status e roteamento de comandos recebidos. Aproximadamente 500 linhas de código padrão desaparecem em `link.begin(); link.loop();`.

## Exemplo mínimo

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

## O que a biblioteca faz

- Conexão WiFi e keep-alive; provisionamento Improv sobre Serial Web para configuração inicial.
- Protocolo de reivindicação: registração do dispositivo no backend, reivindicação de conta via PIN.
- Sessão MQTT com o agente iDryer: TLS, sessão persistente, reconexão automática, sincronização de hora NTP.
- Publicação periódica de telemetria (`Telemetry`) e status (`Status`) em um temporizador.
- Roteamento de comandos recebidos (`commands/invoke`, `commands/set`, `commands/ping`) para o manipulador do produto.
- Servidor WebSocket local: um cliente LAN vê o mesmo fluxo que a nuvem.
- Persistência NVS: credenciais WiFi, token do dispositivo, configuração de menu entre reinicializações.

## O que a biblioteca não faz

- Não gerencia hardware do produto: ventiladores, aquecedores, fitas LED, sensores.
- Não contém lógica de negócios para secagem, armazenamento ou iluminação.
- Não sabe sobre parâmetros de menu específicos do produto — apenas os transporta.
- Não publica telemetria sem dados do produto: você preenche `link.telemetry.*` você mesmo em `loop()`.

## Para onde ir a seguir

- [Comece em 5 minutos](../02-quickstart/01-five-minutes.md)
- [API completa: iDryer::Link](../03-public-api/01-link-api-reference.md)
