---
title: "Cartão do dispositivo: o card manifest"
description: "Como um firmware sobre idryer-core descreve o seu cartão com s_link.card(): sensores, ações com parâmetros de arranque, o que segue para o portal e o que o portal e a aplicação desenham."
---

# Cartão do dispositivo: o card manifest

O portal e a aplicação móvel constroem o cartão de qualquer dispositivo a partir do seu **card manifest** — uma descrição que o firmware publica sobre si próprio: o que mostrar e o que pode ser controlado. Um novo tipo de dispositivo não precisa de código no portal nem na aplicação.

O manifesto faz parte da fachada `iDryer::Link` (`s_link.card()`). O runtime de baixo nível (`IdryerRuntime`) não o publica.

O menu e o cartão são coisas diferentes. O menu espelha as definições do dispositivo: um valor alterado no portal é escrito na memória do dispositivo. O cartão mostra medições e lança operações: os parâmetros de arranque seguem com um comando e não são escritos no menu. Um parâmetro de arranque pode obter os limites e o valor por omissão de um item do menu.

---

## Como funciona

```text
firmware: declarações s_link.card()
   │  o core constrói o JSON e publica-o retained (QoS 1) em idryer/{key}/card
   ▼
backend do portal: valida o manifesto (limites, tipos e campos permitidos), guarda-o
   ▼
portal e aplicação: desenham o cartão
   │  o utilizador carrega num botão
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
o core encaminha "card.<id>" para o seu callback
```

O core publica o manifesto depois de a ligação MQTT estar estabelecida e volta a publicá-lo quando muda a declaração ou um item do menu de que ela depende.

---

## Entidades: o que mostrar

Os sensores do vocabulário do ecossistema são adicionados pelas flags de `Config`; não os declara:

| Flag de `Config` | Célula do cartão |
|---|---|
| `hasAirTemp` | temperatura do ar |
| `hasAirHumidity` | humidade |
| `hasHeaterTemp` | temperatura do aquecedor |
| `hasHeater` | potência de aquecimento |
| `hasFan` | ventilador ligado / desligado |
| `hasServo` | registo aberto / fechado |
| `hasWeight` | módulos de pesagem (topic `weights`) |
| `hasRfid` | a unidade tem leitor RFID |

Valor próprio: adicione-o à telemetria e declare um sensor com o caminho JSON:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Os controlos simples também são entidades: `button`, `number`, `select`. O valor é enviado assim que o utilizador o altera e o core chama o seu callback:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Ações: operações com parâmetros de arranque

Uma ação é uma operação do dispositivo: iniciar a secagem, aquecer, ligar a luz, parar. Tem um **modo** — o modo da unidade depois da ação (`status.units[].mode`) — e **parâmetros** com um significado (`purpose`). O cartão decide o que mostrar pelo modo atual da unidade:

- o modo da unidade coincide com o modo de uma ação → o dispositivo está ocupado com ela: bloco de sessão e botão Parar (a ação com modo `IDLE`);
- caso contrário → o formulário de arranque; várias ações de arranque → um seletor de modo.

Exemplo: um armário de armazenamento aquecido com um ESP32. A temperatura alvo é o item do menu `target_temp` (30–50 °C, por omissão 45).

```cpp
#include <menu_meta.h>
#include <menu_cache.h>
#include <menu_bindings.h>          // menu_sync_state_to_cache
#include <card/card_menu_bridge.h>

static void onStorage(uint8_t unit, JsonObjectConst args) {
    s_targetC = args["temperature"].as<float>();   // já dentro de 30..50
    s_link.status.mode[unit]        = iDryer::UnitMode::Storage;
    s_link.status.targetTempC[unit] = s_targetC;
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.publishStatusNow();
}

void setup() {
    menu.initDefaults();
    menu.loadFromNVS();
    menu_sync_state_to_cache();     // valores do menu na cache que o cartão lê
    s_link.begin();

    auto& card = s_link.card();
    idryer::card_menu::attach(card);
    card.action("storage", "STORAGE", onStorage)
        .name("ru", "Хранение").name("en", "Storage")
        .param("temperature", "target_temperature", MENU_TARGET_TEMP);
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");
}
```

O que o callback recebe:

- os números são limitados aos limites da unidade; um número em falta é substituído pelo valor por omissão;
- `unit` — índice da unidade a partir de `unitId` (`"U1"` → 0); um comando para uma unidade que o dispositivo não tem é ignorado;
- depois do arranque, defina `status.mode` e chame `publishStatusNow()` — pelo estado, o cartão passa para o bloco de sessão.

### API das ações

| Chamada | O que faz |
|---|---|
| `card.action(id, mode, cb)` | uma ação; `mode` — modo da unidade a seguir, `nullptr` — o modo não muda |
| `.param(id, purpose, MENU_ID)` | um número: limites, passo, valor por omissão e unidade do item do menu |
| `.param(id, purpose, min, max, step, def[, unit])` | um número com limites próprios |
| `.ceiling(MENU_ID)` | o limite superior do parâmetro anterior é o valor desse item do menu (por exemplo, a temperatura máxima do ar) |
| `.stages(id, "stages", MENU_ID)` | etapas do perfil `[{temperature, ramp, hold}]`, segundos; temperatura da etapa dentro dos limites do item do menu |
| `.select(id, purpose, options, count[, def])` | escolha numa lista; um valor fora da lista é substituído pelo valor por omissão |
| `.color(id, purpose, "#FFFFFF")` | uma cor `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | botões fixos do cabeçalho: localizar o dispositivo, limpar erros |
| `.name("ru", "…").name("en", "…")` | o nome da ação, quando o cartão não conhece o modo |

Valores de `purpose`: `target_temperature`, `target_humidity`, `duration` (minutos, 0 — sem limite), `stages`, `start_stage` (a partir de 0), `effect`, `rgb_color`. O cartão usa-os para os rótulos dos campos e para funções do portal: o preset de secagem preenche `target_temperature` e `duration` de uma ação com modo `DRYING`, um perfil de secagem preenche `stages`.

Os callbacks são funções ou lambdas sem capturas. As strings de `name` e as opções de seleção são guardadas como ponteiros — use literais ou arrays estáticos.

---

## O que segue para o portal

O armário acima publica (`Config`: `hasAirTemp`, `hasAirHumidity`, `hasHeaterTemp`, `hasHeater`, `hasFan`):

```json
{
  "v": 2,
  "entities": [
    {"id": "temp", "type": "sensor", "device_class": "temperature", "unit": "°C", "source": "telemetry", "path": "units[0].temperature"},
    {"id": "humidity", "type": "sensor", "device_class": "humidity", "unit": "%", "source": "telemetry", "path": "units[0].humidity"},
    {"id": "heater_temp", "type": "sensor", "device_class": "heater_temp", "unit": "°C", "source": "telemetry", "path": "units[0].heaterTemp"},
    {"id": "power", "type": "sensor", "device_class": "power", "unit": "%", "source": "telemetry", "path": "units[0].heaterPower"},
    {"id": "fan", "type": "binary_sensor", "device_class": "fan", "source": "telemetry", "path": "units[0].fanStatus"}
  ],
  "actions": [
    {"id": "storage", "mode": "STORAGE", "name": {"ru": "Хранение", "en": "Storage"}, "action": "card.storage",
     "params": [{"id": "temperature", "purpose": "target_temperature", "type": "number",
                 "limits": [30, 50], "step": 1, "default": 45, "unit": "°C"}]},
    {"id": "stop", "mode": "IDLE", "name": {"ru": "Стоп", "en": "Stop"}, "action": "card.stop"}
  ]
}
```

`limits` e `default` vieram do item do menu. Com `.ceiling()`, o parâmetro recebe ainda `max_by` — o título do item do menu que cortou o limite superior; o cartão indica-o quando um valor está acima do limite.

---

## O que o portal e a aplicação desenham

Esquema, não captura de ecrã. Em repouso:

```text
┌─ DIY Storage Cabinet ─────────────── [Ocioso] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                  │  ← temperatura, humidade, aquecedor
│ | 0 %     | desligado       |                  │  ← potência, ventilador (cinzentos: 0 / desligado)
│ [Temp. 45 °C          ]  [Armazenamento]       │  ← ação "storage"
└────────────────────────────────────────────────┘
```

Depois do arranque, o dispositivo reporta o modo `STORAGE` e o mesmo cartão mostra o bloco de sessão (temperatura alvo e tempo em armazenamento) e o botão Parar — a ação com modo `IDLE`.

Regras dos dois lados:

- uma célula fica cinzenta quando não há valor; a potência, também com 0; o ventilador e o registo, no estado desligado / fechado;
- um valor fora dos limites não é substituído: o arranque fica bloqueado e o motivo aparece por baixo do formulário;
- num firmware com ações no manifesto, os botões localizar e limpar erros só existem se ele declarar ações com `identify` / `clear_errors`.

Onde o cartão aparece:

| | Portal | Aplicação |
|---|---|---|
| um tipo de dispositivo que não é um produto iDryer | cartão do dashboard a partir do manifesto, com as ações; página do dispositivo — leituras e ações (se o manifesto tiver ações) | início — leituras; ações — na página do dispositivo |
| `deviceType = Dryer` | o cartão do secador com as mesmas ações do manifesto | início — monitorização; ações — na página do dispositivo |

---

## Limites

| | Core | Backend do portal |
|---|---|---|
| entidades | 16 declaradas + automáticas | 32 |
| linhas de layout | 8 | 16 |
| ids por linha | 4 | 4 |
| ações | 8 | 8 |
| parâmetros por ação | 4 (`IDRYER_CARD_MAX_PARAMS`) | 6 |
| idiomas de `name` | `ru`, `en` | até 4 |

`id`: letras latinas minúsculas, dígitos e `_`, até 24 caracteres; entidades e ações partilham um espaço de nomes. O documento do manifesto está limitado a 4096 bytes; se não couber, o core escreve `manifest overflows` no log e não o publica.

O formato em si — `contracts/mqtt_contract.yaml`, secção `mqtt_only`, `suffix: card`.
