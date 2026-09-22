---
title: "Cartão do dispositivo: o card manifest"
description: "Como um firmware sobre idryer-core descreve seu cartão com s_link.card(): sensores, ações com parâmetros de partida, o que vai para o portal e o que o portal e o app desenham."
---

# Cartão do dispositivo: o card manifest

O portal e o app móvel montam o cartão de qualquer dispositivo a partir do seu **card manifest** — uma descrição que o firmware publica sobre si mesmo: o que mostrar e o que pode ser controlado. Um novo tipo de dispositivo não precisa de código no portal nem no app.

O manifesto faz parte da fachada `iDryer::Link` (`s_link.card()`). O runtime de baixo nível (`IdryerRuntime`) não o publica.

Menu e cartão são coisas diferentes. O menu espelha as configurações do dispositivo: um valor alterado no portal é gravado na memória do dispositivo. O cartão mostra medições e dispara operações: os parâmetros de partida vão junto com um comando e não são gravados no menu. Um parâmetro de partida pode pegar os limites e o valor padrão de um item do menu.

---

## Como funciona

```text
firmware: declarações s_link.card()
   │  o core monta o JSON e publica retained (QoS 1) em idryer/{key}/card
   ▼
backend do portal: valida o manifesto (limites, tipos e campos permitidos), salva
   ▼
portal e app: desenham o cartão
   │  o usuário toca em um botão
   ▼
idryer/{key}/commands/invoke  {"unitId":"U1","action":"card.<id>","args":{…}}
   ▼
o core encaminha "card.<id>" para o seu callback
```

O core publica o manifesto depois que a conexão MQTT sobe e publica de novo quando muda a declaração ou um item do menu do qual ela depende.

---

## Entidades: o que mostrar

Os sensores do vocabulário do ecossistema entram pelas flags de `Config`; você não os declara:

| Flag de `Config` | Célula do cartão |
|---|---|
| `hasAirTemp` | temperatura do ar |
| `hasAirHumidity` | umidade |
| `hasHeaterTemp` | temperatura do aquecedor |
| `hasHeater` | potência de aquecimento |
| `hasFan` | ventilador ligado / desligado |
| `hasServo` | damper aberto / fechado |
| `hasWeight` | módulos de pesagem (topic `weights`) |
| `hasRfid` | a unidade tem leitor RFID |

Valor próprio: coloque na telemetria e declare um sensor com o caminho JSON:

```cpp
s_link.onTelemetryPublish([](JsonObject root) {
    root["units"][0]["co2ppm"] = g_co2;
});
s_link.card().sensor("co2", "CO2", "ppm", "units[0].co2ppm");
```

Controles simples também são entidades: `button`, `number`, `select`. O valor é enviado assim que o usuário o altera, e o core chama o seu callback:

```cpp
static const char* kModes[] = { "auto", "on", "off" };
s_link.card().select("mode", "Mode", kModes, 3, [](const char* opt) { onMode(opt); });
s_link.card().number("threshold", "Threshold", 100, 400, 10, "", [](float v) { onThreshold(v); });
s_link.card().button("purge", "Purge", []() { onPurge(); });
```

---

## Ações: operações com parâmetros de partida

Uma ação é uma operação do dispositivo: iniciar a secagem, aquecer, ligar a luz, parar. Ela tem um **modo** — o modo da unidade depois da ação (`status.units[].mode`) — e **parâmetros** com um significado (`purpose`). O cartão decide o que mostrar pelo modo atual da unidade:

- o modo da unidade é igual ao modo de uma ação → o dispositivo está ocupado com ela: bloco de sessão e botão Parar (a ação com modo `IDLE`);
- senão → o formulário de partida; várias ações de partida → um seletor de modo.

Exemplo: um armário de armazenamento aquecido com um ESP32. A temperatura alvo é o item de menu `target_temp` (30–50 °C, padrão 45).

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
    menu_sync_state_to_cache();     // valores do menu no cache que o cartão lê
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

- os números vêm limitados aos limites da unidade; um número ausente é trocado pelo valor padrão;
- `unit` — índice da unidade a partir de `unitId` (`"U1"` → 0); um comando para uma unidade que o dispositivo não tem é ignorado;
- depois da partida, defina `status.mode` e chame `publishStatusNow()` — pelo status, o cartão passa para o bloco de sessão.

### API de ações

| Chamada | O que faz |
|---|---|
| `card.action(id, mode, cb)` | uma ação; `mode` — modo da unidade depois dela, `nullptr` — o modo não muda |
| `.param(id, purpose, MENU_ID)` | um número: limites, passo, valor padrão e unidade vindos do item do menu |
| `.param(id, purpose, min, max, step, def[, unit])` | um número com limites próprios |
| `.ceiling(MENU_ID)` | o limite superior do parâmetro anterior é o valor desse item do menu (por exemplo, a temperatura máxima do ar) |
| `.stages(id, "stages", MENU_ID)` | etapas do perfil `[{temperature, ramp, hold}]`, segundos; temperatura da etapa dentro dos limites do item do menu |
| `.select(id, purpose, options, count[, def])` | escolha em uma lista; um valor fora da lista vira o valor padrão |
| `.color(id, purpose, "#FFFFFF")` | uma cor `#RRGGBB` |
| `.deviceClass("identify")`, `.deviceClass("clear_errors")` | botões fixos do cabeçalho: localizar o dispositivo, limpar erros |
| `.name("ru", "…").name("en", "…")` | o nome da ação, quando o cartão não conhece o modo |

Valores de `purpose`: `target_temperature`, `target_humidity`, `duration` (minutos, 0 — sem limite), `stages`, `start_stage` (a partir de 0), `effect`, `rgb_color`. O cartão usa esses valores para os rótulos dos campos e para recursos do portal: o preset de secagem preenche `target_temperature` e `duration` de uma ação com modo `DRYING`, um perfil de secagem preenche `stages`.

Callbacks são funções ou lambdas sem captura. As strings de `name` e as opções de seleção são guardadas como ponteiros — use literais ou arrays estáticos.

---

## O que vai para o portal

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

`limits` e `default` vieram do item do menu. Com `.ceiling()`, o parâmetro recebe também `max_by` — o título do item do menu que cortou o limite superior; o cartão cita esse nome quando um valor passa do limite.

---

## O que o portal e o app desenham

Esquema, não captura de tela. Em repouso:

```text
┌─ DIY Storage Cabinet ─────────────── [Ocioso] ─┐
│ | 24.8 °C | 41 %  | 25.1 °C |                  │  ← temperatura, umidade, aquecedor
│ | 0 %     | desligado       |                  │  ← potência, ventilador (cinza: 0 / desligado)
│ [Temp. 45 °C          ]  [Armazenamento]       │  ← ação "storage"
└────────────────────────────────────────────────┘
```

Depois da partida, o dispositivo informa o modo `STORAGE` e o mesmo cartão mostra o bloco de sessão (temperatura alvo e tempo em armazenamento) e o botão Parar — a ação com modo `IDLE`.

Regras dos dois lados:

- a célula fica cinza quando não há valor; a potência, também com 0; o ventilador e o damper, no estado desligado / fechado;
- um valor fora dos limites não é trocado: a partida fica bloqueada e o motivo aparece abaixo do formulário;
- em firmware com ações no manifesto, os botões localizar e limpar erros só existem se ele declarar ações com `identify` / `clear_errors`.

Onde o cartão aparece:

| | Portal | App |
|---|---|---|
| um tipo de dispositivo que não é produto iDryer | cartão do dashboard a partir do manifesto, com as ações; página do dispositivo — leituras e ações (se o manifesto tiver ações) | início — leituras; ações — na página do dispositivo |
| `deviceType = Dryer` | o cartão do secador com as mesmas ações do manifesto | início — monitoramento; ações — na página do dispositivo |

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

`id`: letras latinas minúsculas, dígitos e `_`, até 24 caracteres; entidades e ações compartilham um espaço de nomes. O documento do manifesto é limitado a 4096 bytes; se não couber, o core escreve `manifest overflows` no log e não o publica.

O formato em si — `contracts/mqtt_contract.yaml`, seção `mqtt_only`, `suffix: card`.
