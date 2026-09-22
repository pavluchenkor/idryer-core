# Menu como Protocolo: menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## Três ficheiros, três papéis

| Ficheiro | Dono | Descreve |
|------|-------|-----------|
| `src/menu/menu.yaml` | o seu produto | menu do dispositivo: parâmetros, ações, estrutura |
| `contracts/mqtt_contract.yaml` | idryer-core | lista de significados conhecidos: `canonical_roles` com rótulos em vários idiomas |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | gerado | tipos TypeScript para o portal |

**`role:`** — nome semântico de um item do menu. O firmware diz «tenho `iheater.heat_temp`» e não «tenho o item número 35». Os nomes internos do firmware podem mudar; `role:` mantém-se.

O menu espelha as definições do dispositivo. O firmware publica todos os itens de `menu.yaml`, com ou sem `role:`. O portal desenha cada item conforme o seu tipo: valor, interruptor, ação, submenu. `role:` dá ao item um rótulo do contrato no idioma do utilizador; sem `role:`, o portal mostra o nome enviado pelo dispositivo.

O menu não constrói o cartão do dispositivo. O que o cartão mostra e as operações que lança são descritos pelo card manifest — ver [Cartão do dispositivo: o card manifest](../09-add-product/02-add-widget.md). Uma ação do cartão pode obter os limites e o valor por omissão do seu parâmetro a partir de um item do menu.

---

## 1. Compilação do firmware (`pio run`)

`menu.yaml` → `menu_gen.py` verifica cada `role:` em `canonical_roles` do contrato → se uma role for desconhecida, a compilação falha com um erro e a lista de roles válidas → o gerador escreve os ficheiros C++ em `src/menu/`.

## 2. Atualizar o TypeScript do portal (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` gera `mqtt-api.types.ts` e os rótulos das roles `roles.{lang}.json` → os ficheiros são copiados para o portal.

Execute quando o contrato mudar. Faça commit do resultado.

## 3. Em execução: dispositivo ↔ portal

O firmware publica o menu no topic `config` (é o código do produto que o faz com o comando `get_config`; os produtos iDryer também quando ficam online) → o backend do portal guarda-o → o portal obtém-no com `GET /devices/:id/menu-config` → cada item é desenhado pelo seu tipo `t` (`val`, `tog`, `act`, `sub`); o rótulo é `canonical_roles[r].labels[lang]`, depois o inglês, depois o nome `n` enviado pelo dispositivo.

Os parâmetros (`min`, `max`, `val`) vêm do próprio item do menu — o firmware conhece os valores atuais.

O portal altera um valor com `commands/set { "id": <id>, "val": <value> }`.

---

## Como adicionar uma definição (parâmetro NVS)

```yaml
- id: my_param
  type: value
  role: my.param        # opcional: um rótulo do contrato
  title: { ru: "ПАРАМЕТР", en: "PARAM" }
  unit: { ru: "°C", en: "°C" }
  vtype: uint16
  min: 0
  max: 100
  step: 1
  bind: my_param        # chave NVS (≤ 15 caracteres)
  persist: true
  scope: global
  default: 50
```

`bind` = chave NVS. `persist: true` = o valor sobrevive a um reinício.

`role:` não é um campo livre: o valor tem de estar em `canonical_roles` do contrato; caso contrário, a compilação falha. A lista está em `contracts/mqtt_contract.yaml` → `canonical_roles` ou em `menu.template.yaml`. Uma role nova é primeiro adicionada ao contrato e depois executa-se `regen.sh`.

---

## Como adicionar uma operação ao cartão do dispositivo

As operações (arrancar, parar, aquecer, iluminar) declaram-se como ações do cartão, não como itens do menu. Os seus limites podem vir de um item do menu:

```cpp
idryer::card_menu::attach(s_link.card());
s_link.card().action("storage", "STORAGE", onStorage)
    .param("temperature", "target_temperature", MENU_TARGET_TEMP);
s_link.card().action("stop", "IDLE", onStop);
```

A descrição completa — [Cartão do dispositivo: o card manifest](../09-add-product/02-add-widget.md).

---

## O que NÃO fazer

- Não adicione `widget:` ao `menu.yaml`. O campo `widget` de `canonical_roles` é apenas de referência: nem o portal nem a aplicação o leem.
- Não edite `mqtt-api.types.ts` à mão — é gerado por `regen.sh`.
- Não mexa nas flags `Config.hasXxx` para ações novas — servem apenas para telemetria (sensores, estados).
