# Menu como Protocolo: menu.yaml ↔ mqtt_contract.yaml ↔ Portal

---

## Três ficheiros — três papéis

| Ficheiro | Proprietário | Descreve |
|------|-------|-----------|
| `src/menu/menu.yaml` | seu produto | menu do dispositivo: parâmetros, ações, estrutura |
| `contracts/mqtt_contract.yaml` | idryer-core | lista de significados conhecidos: o que cada `role:` significa e como o portal a exibe |
| `frontend-v2/src/contracts/mqtt-api.types.ts` | gerado | tipos TypeScript para o portal |

**`role:`** — um nome semântico para um item de menu. O firmware diz "tenho `iheater.heat_start`" em vez de "tenho botão número 35". Este é o contrato estável entre o dispositivo e o portal — nomes internos de firmware podem mudar, `role:` permanece fixo.

**Widget** — como o portal exibe este item: um botão, controlo deslizante, alternância ou um componente complexo (seletor de cores, editor de perfil). Determinado pelo contrato via `role:`, não pelo firmware.

Um item de menu com `role:` é visível ao portal. Sem `role:` — privado, mostrado apenas no ecrã do dispositivo.

---

## 1. Compilação do firmware (`pio run`)

`menu.yaml` → `pre_gen_menu.py` valida cada `role:` contra `canonical_roles` no contrato → se uma função for desconhecida, a compilação falha com um erro e uma lista de funções válidas → `menu_gen.py` gera ficheiros C++ em `src/menu/`

A validação é integrada na etapa de compilação — é fisicamente impossível usar uma função inexistente silenciosamente.

## 2. Atualização de TypeScript para o portal (`regen.sh`)

`mqtt_contract.yaml` → `gen_ts_types.py` gera `mqtt-api.types.ts` → o ficheiro é copiado para `frontend-v2/src/contracts/`

Execute manualmente quando o contrato mudar. Confirme o resultado.

## 3. Runtime: dispositivo ↔ portal

O dispositivo conecta → publica menu no tópico MQTT `config` → portal lê cada item com campo `r:` → procura `CanonicalRoles[r].widget` → renderiza widget a partir de `WIDGET_REGISTRY`.

Os parâmetros (`min`, `max`, `val`) vêm do item de menu em si — o firmware conhece os valores atuais.

---

## Como adicionar uma nova ação ao painel do portal

`role:` não é um campo de forma livre. O valor deve vir da lista fechada em `canonical_roles` no contrato. Não pode inventar uma função instantaneamente — a compilação falhará. Veja funções disponíveis em `contracts/mqtt_contract.yaml` → secção `canonical_roles`, ou em `menu.template.yaml`.

**1. Escolha uma função do contrato.** Se nenhuma se adequar — adicione-a a `mqtt_contract.yaml` → `canonical_roles` primeiro, depois execute `regen.sh`:

```yaml
canonical_roles:
  my.action: { type: action, widget: button }
```

**2. Adicione um item a `menu.yaml`:**

```yaml
- id: my_action
  type: action
  role: my.action
  title: { ru: "МОЁ ДЕЙСТВИЕ", en: "MY ACTION" }
```

**3. Manipule-o em firmware (`main.cpp`):**

```cpp
if (action == "my.action") { /* do the thing */ }
```

`pio run` → validação → C++ → firmware publica `r: "my.action"` → portal renderiza um botão.

---

## Como adicionar uma configuração (parâmetro NVS)

```yaml
- id: my_param
  type: value
  role: my.param        # apenas se deve aparecer no portal; omita para apenas exibição
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

`bind` = chave NVS. `persist: true` = valor sobrevive à reinicialização.
O portal muda o valor via `commands/set { "id": <id>, "val": <value> }`.

---

## O que NÃO fazer

- Não adicione `widget:` a `menu.yaml` — o widget é determinado pelo contrato via `role:`, não pelo firmware
- Não edite `mqtt-api.types.ts` manualmente — é gerado por `regen.sh`
- Não toque em sinalizadores `Config.hasXxx` para novas ações — estes são apenas para telemetria (sensores, estados)
