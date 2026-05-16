# Adicionar um Widget e um Novo Dispositivo

Ciclo completo: desde a bifurcação do repositório até ao PR mesclado. Abrange firmware, contrato, widget React e testes de portal.

Se você apenas precisar de firmware sem um novo widget — consulte [01-add-new-product.md](01-add-new-product.md).

---

## Pré-requisitos

- Python 3.9+ com `pip install pyyaml jsonschema`
- Node.js 18+
- PlatformIO CLI
- Acesso ao portal iDryer para testes UIKit

---

## Passo 1. Bifurcar e Clonar

1. Bifurque o repositório `idryer-core` no GitHub.
2. Clone sua bifurcação localmente:

    ```bash
    git clone https://github.com/<your-username>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. Verifique se o contrato passa na validação no estado atual:

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## Passo 2. Editar o Contrato

Todas as alterações vão para `contracts/mqtt_contract.yaml`. Mantenha tudo em um único changeset.

!!! aviso
    Não edite ficheiros em `_generated/` — são sobrescritos pelos geradores.

### 2a. Vocabulário de capacidades (novo tipo de periférico)

Se o dispositivo tiver um novo tipo de hardware (por exemplo, um sensor de CO2), adicione uma entrada na secção `capability_vocabulary`:

```yaml
capability_vocabulary:
  co2:
    description: "CO2 sensor (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

Isto adiciona automaticamente o campo `hasAirCo2: bool` a `iDryer::Config` na próxima regeneração.

### 2b. Funções canónicas (nova função + widget)

Se o dispositivo expõe um novo item de menu, registar a função em `canonical_roles`:

```yaml
canonical_roles:
  co2.read:
    type: float
    widget: Co2Display
    unit: ppm
    labels:
      ru: "CO₂"
      en: "CO₂"
```

O valor `widget` é o nome do componente React que você escreverá no Passo 5.

### 2c. Ações de invocação (se o widget envia comandos)

Se o widget acionara uma ação no dispositivo, descreva-a em `invoke_actions`:

```yaml
invoke_actions:
  my_device:
    co2.calibrate:
      description: "Start CO2 sensor calibration"
      args:
        targetPpm:
          type: uint16
          description: "Reference CO2 value (ppm)"
          required: true
```

### 2d. Perfil de dispositivo (novo tipo de dispositivo)

Adicione o perfil a `device_profiles`:

```yaml
device_profiles:
  my_device:
    description: "My device"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

Os valores de capacidade vêm do `capability_vocabulary` definido no passo 2a.

---

## Passo 3. Validar e Regenerar

```bash
cd contracts
./regen.sh
```

Sinalizadores:

| Sinalizador | Efeito |
|---|---|
| (nenhum) | Validar + todos os geradores + copiar para portal |
| `--firmware-only` | Apenas geradores de firmware, pular cópia de portal |
| `--help` | Mostrar ajuda |

No sucesso, `_generated/` é atualizado com:

- `uart_protocol.h`, `mqtt_topics.h` — cabeçalhos C++
- `iDryer_api.h` — fachada Config/DeviceType
- `mqtt-api.types.ts` — tipos TypeScript
- `scaffolds/my_device/` — esqueleto do projeto PlatformIO
- No portal: ficheiros em `src/components/widgets/`

Se `regen.sh` sair com um erro, corrija o problema antes de continuar.

---

## Passo 4. Implementar Firmware

Use o projeto de andaime gerado:

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

Preencha as secções TODO em `src/main.cpp`:

- `onOnline()` — carregue a configuração do NVS, inicialize o hardware.
- `loop()` — sonde sensores, chame `s_runtime.publishTelemetry(tel)`.
- `buildInfoJson()` — já preenchido pelo gerador a partir de capacidades.
- `onInvoke()` — manipule `co2.calibrate`.

Para detalhes, consulte [01-add-new-product.md](01-add-new-product.md).

---

## Passo 5. Criar o Widget React

Os widgets vivem em `contracts/widgets/` e são copiados para o portal por `regen.sh`.

!!! nota
    Não edite widgets diretamente em `portal/src/components/widgets/` — serão sobrescritos na próxima execução de `regen.sh`. Edite apenas em `contracts/widgets/`.

### Criar o ficheiro de widget

```tsx
// contracts/widgets/Co2Display.tsx
import type { WidgetProps } from "./widget-props";

export function Co2DisplayWidget({ device }: WidgetProps) {
  const unit = device.units[0];
  const co2 = unit?.co2Ppm ?? null;
  return (
    <div style={{ padding: "8px 16px" }}>
      {co2 !== null ? `${co2} ppm` : "—"}
    </div>
  );
}
```

### Registar em index.ts

```ts
// contracts/widgets/index.ts
export { Co2DisplayWidget } from "./Co2Display";
```

### Registar em widget-registry.tsx (no portal)

Após a próxima execução de `regen.sh`, o ficheiro aparecerá em `portal/src/components/widgets/Co2Display.tsx`. Adicione uma entrada a `widget-registry.tsx` manualmente:

```tsx
import { Co2DisplayWidget } from "./Co2Display";

export const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  // ...
  Co2Display: Co2DisplayWidget,
};
```

---

## Passo 6. Testar em UIKit

Abra `portal/src/pages/UiKitPage.tsx` e adicione uma secção com dados simulados dentro do grupo **Device Dashboard Widgets**:

```tsx
<KitSection title="Co2Display">
  <Co2DisplayWidget device={MOCK_DEVICE} item={MOCK_CO2_ITEM} socket={null} />
</KitSection>
```

Abra o portal localmente e navegue para `/uikit` — o widget deve renderizar sem um login.

---

## Passo 7. Checklist de PR

Antes de submeter o PR, verifique que:

- [ ] `./contracts/regen.sh` completa sem erros
- [ ] `_generated/*` é confirmado (não em `.gitignore`)
- [ ] `contracts/widgets/` — novo ficheiro de widget adicionado
- [ ] `contracts/widgets/index.ts` — widget exportado
- [ ] `widget-registry.tsx` no portal — widget registado
- [ ] O widget renderiza em `/uikit` sem erros de consola
- [ ] O andaime em `_generated/scaffolds/my_device/` reflete corretamente as capacidades
- [ ] A descrição do PR declara: propósito do dispositivo, capacidades, nome do widget

Submeta o PR contra o ramo `main` do repositório `idryer-core`.

---

## Todas as Alterações em Um PR

| Ficheiro | Tipo de mudança |
|---|---|
| `contracts/mqtt_contract.yaml` | Fonte de verdade |
| `contracts/_generated/*` | Auto-gerado — confirmado integralmente |
| `contracts/widgets/MyWidget.tsx` | Novo ficheiro |
| `contracts/widgets/index.ts` | +1 linha de exportação |
| *(portal, após `regen.sh`)* | `src/components/widgets/MyWidget.tsx` — cópia |
| *(portal, manual)* | `src/components/widgets/widget-registry.tsx` — +1 entrada |
| *(portal, manual)* | `src/pages/UiKitPage.tsx` — +1 secção em KitGroup |
