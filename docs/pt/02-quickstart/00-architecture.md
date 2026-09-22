# Como funciona o idryer-core

O idryer-core é uma biblioteca para ESP32. Trata de tudo o que liga um dispositivo ao portal e à aplicação:

- Wi-Fi: a rede chega da aplicação iDryer (ESPTouch) ou de uma página web por USB (Improv);
- a associação a uma conta com um token de uso único, e a desassociação;
- uma sessão MQTT segura com religação;
- acesso pela rede local: a aplicação controla o dispositivo mesmo sem internet;
- a publicação de telemetria e estado, a entrega de comandos;
- a atualização do firmware pelo ar;
- o cartão do dispositivo no portal e na aplicação.

Só escreve a sua parte: ler sensores, comandar cargas, declarar o que o cartão mostra e que operações inicia.

## Um único ponto de entrada: `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // o seu próprio dispositivo
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // célula de temperatura
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // o seu código
}
```

| Você | O núcleo |
|---|---|
| preenche `s_link.telemetry.*` | publica a cada 30 s, a cada 60 s em repouso |
| altera `s_link.status.*` (modo, setpoint, tempo) e chama `publishStatusNow()` | entrega o estado; o cartão muda conforme o modo |
| declara `s_link.card()`: sensores, controlos, ações | constrói o card manifest e publica-o |
| regista `s_link.onCommand(...)` | passa os comandos da nuvem e da rede local |

## Cartão do dispositivo

O portal e a aplicação desenham o cartão a partir do card manifest enviado pelo dispositivo:

- as flags `Config.has*` dão células prontas: temperatura, humidade, potência de aquecimento, ventilador e outras;
- `card().sensor(...)` acrescenta um valor seu pelo caminho na telemetria;
- `card().action(...)` declara uma operação: o modo da unidade depois dela e os parâmetros de arranque.

Não é preciso código no portal. Detalhes: [Cartão do dispositivo: o card manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml` é a fonte da verdade

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) descreve o protocolo: tópicos, campos de telemetria e de estado, capacidades dos dispositivos, o card manifest. A partir dele geram-se:

| O quê | Onde |
|---|---|
| `iDryer::Config` (flags `has*`) e estruturas da API | `src/_generated/iDryer_api.h` |
| tópicos MQTT | `contracts/_generated/mqtt_topics.h` |
| protocolo UART ESP32 ↔ controlador | `contracts/_generated/uart_protocol.h` |
| tipos TypeScript para o portal | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Não edite à mão os ficheiros em `_generated/`: `contracts/regen.sh` reescreve-os a partir do contrato.

Um valor seu não exige alterar o contrato: declara-o `card().sensor(...)`. O contrato muda quando todos os produtos precisam de uma capacidade nova: primeiro `mqtt_contract.yaml`, depois `regen.sh`, depois o código.

## Produtos sobre o núcleo

- **iDryer Link**: módulo de comunicação do secador, um ESP32 junto ao controlador, troca por UART.
- **iDryer Storage**: iluminação de uma estante de bobinas, fita endereçável e sensor SHT31.
- **iHeater Link**: controlo do aquecedor iHeater, integrações com Bambu Lab, Klipper/Moonraker e Home Assistant.

## A seguir

[Arrancar em 5 minutos](01-five-minutes.md).
