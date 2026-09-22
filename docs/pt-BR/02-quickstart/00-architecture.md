# Como o idryer-core funciona

O idryer-core é uma biblioteca para ESP32. Ele cuida de tudo o que liga um dispositivo ao portal e ao app:

- Wi-Fi: a rede vem do app iDryer (ESPTouch) ou de uma página web pela USB (Improv);
- a vinculação a uma conta com um token de uso único, e a desvinculação;
- uma sessão MQTT segura com reconexão;
- acesso pela rede local: o app controla o dispositivo mesmo sem internet;
- a publicação de telemetria e status, a entrega de comandos;
- a atualização de firmware pelo ar;
- o card do dispositivo no portal e no app.

Você escreve só a sua parte: ler sensores, acionar cargas, declarar o que o card mostra e quais operações ele inicia.

## Um único ponto de entrada: `iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // seu próprio dispositivo
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
    s_link.telemetry.airTempC[0] = readTemperature();   // seu código
}
```

| Você | O núcleo |
|---|---|
| preenche `s_link.telemetry.*` | publica a cada 30 s, a cada 60 s em repouso |
| muda `s_link.status.*` (modo, setpoint, tempo) e chama `publishStatusNow()` | entrega o status; o card muda conforme o modo |
| declara `s_link.card()`: sensores, controles, ações | monta o card manifest e publica |
| registra `s_link.onCommand(...)` | repassa comandos da nuvem e da rede local |

## Card do dispositivo

O portal e o app desenham o card a partir do card manifest que o dispositivo envia:

- as flags `Config.has*` dão células prontas: temperatura, umidade, potência de aquecimento, ventilador e outras;
- `card().sensor(...)` acrescenta um valor seu pelo caminho na telemetria;
- `card().action(...)` declara uma operação: o modo da unidade depois dela e os parâmetros de partida.

Não precisa de código no portal. Detalhes: [Card do dispositivo: o card manifest](../09-add-product/02-add-widget.md).

## `mqtt_contract.yaml` é a fonte da verdade

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) descreve o protocolo: tópicos, campos de telemetria e de status, capacidades dos dispositivos, o card manifest. A partir dele são gerados:

| O quê | Onde |
|---|---|
| `iDryer::Config` (flags `has*`) e estruturas da API | `src/_generated/iDryer_api.h` |
| tópicos MQTT | `contracts/_generated/mqtt_topics.h` |
| protocolo UART ESP32 ↔ controlador | `contracts/_generated/uart_protocol.h` |
| tipos TypeScript para o portal | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Não edite à mão os arquivos em `_generated/`: o `contracts/regen.sh` reescreve eles a partir do contrato.

Um valor seu não exige mudar o contrato: quem declara é `card().sensor(...)`. O contrato muda quando todos os produtos precisam de uma capacidade nova: primeiro `mqtt_contract.yaml`, depois `regen.sh`, depois o código.

## Produtos sobre o núcleo

- **iDryer Link**: módulo de comunicação do secador, um ESP32 ao lado do controlador, troca pela UART.
- **iDryer Storage**: iluminação de uma estante de carretéis, fita endereçável e sensor SHT31.
- **iHeater Link**: controle do aquecedor iHeater, integrações com Bambu Lab, Klipper/Moonraker e Home Assistant.

## Próximo passo

[Rodar em 5 minutos](01-five-minutes.md).
