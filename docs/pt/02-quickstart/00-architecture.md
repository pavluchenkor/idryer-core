# Como o idryer-core funciona

idryer-core é uma biblioteca para ESP32 que trata de toda a pilha em nuvem: provisionamento WiFi via Improv-Serial, protocolo de reivindicação para vincular um dispositivo a uma conta idryer.org, sessão MQTT com TLS com reconexão automática, roteamento de comandos do portal e publicação periódica de telemetria.

Você escreve apenas o que é específico para seu dispositivo: leitura de sensores, acionamento de periféricos. Tudo mais está dentro da biblioteca.

## mqtt_contract.yaml — única fonte de verdade

O arquivo [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) define:

- **capabilities** — quais periféricos cada tipo de dispositivo suporta (aquecedor, fita LED, sensores);
- **telemetry fields** — nomes de campos e tipos de dados em pacotes MQTT;
- **UART protocol** — estruturas entre ESP32 e um co-processador;
- **TypeScript types** — para o frontend do portal.

A partir deste arquivo, o código é gerado automaticamente:

| O que é gerado | Onde |
|---|---|
| `iDryer::Config` (sinalizadores has*) | `src/_generated/iDryer_api.h` |
| Tópicos MQTT (constantes C++) | `contracts/_generated/mqtt_topics.h` |
| Tipos TypeScript | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    Não edite manualmente arquivos em `src/_generated/` e `contracts/_generated/` — eles serão sobrescritos na próxima execução de regeneração.

## Como adicionar novos periféricos

O procedimento é o mesmo para qualquer novo recurso — um botão, um sensor de CO2, um leitor RFID.

**1.** Adicione uma entrada a `capability_vocabulary` em [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml):

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** Execute a regeneração:

```bash
cd contracts
./regen.sh
```

Depois disso, `iDryer::Config` terá um campo `hasCo2` e TypeScript terá `HardwareUnitConfigCapabilities.co2`.

**3.** Defina o sinalizador no `main.cpp` do seu dispositivo:

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** Programe o dispositivo. O portal lerá `co2: true` a partir do tópico MQTT `/info` e exibirá automaticamente o bloco de UI correspondente — não são necessárias alterações no portal.

Para tipos de periféricos ainda não presentes no contrato, abra um PR no repositório idryer-core adicionando uma entrada a `capability_vocabulary`. Após a mesclagem — execute `regen.sh`.

## Dois produtos de produção construídos em cima desta biblioteca

**iDryer Storage Link** — ESP32-C3 com uma fita LED WS2812B e um sensor de temperatura/umidade SHT31.

**iHeater Link** — ESP32-C3 com saída RMT para o aquecedor iHeater, com integrações para Bambu Lab, Klipper/Moonraker e Home Assistant.

Ambos os produtos incluem idryer-core via PlatformIO `lib_deps` e implementam apenas sua lógica específica do produto.

## O que vem a seguir

- [01-wifi.md](01-wifi.md) — conecte o ESP32 ao WiFi usando Improv-Serial.
- [../../../README.md](../../../README.md) — visão geral da biblioteca e referência de geração de código.
