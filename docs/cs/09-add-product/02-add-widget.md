# Přidejte widget a nové zařízení

Úplný cyklus: od forku úložiště k sloučenému PR. Pokrývá firmware, smlouvu, widget React a testování portálu.

Pokud potřebujete pouze firmware bez nového widgetu — viz [01-add-new-product.md](01-add-new-product.md).

---

## Požadavky

- Python 3.9+ s `pip install pyyaml jsonschema`
- Node.js 18+
- PlatformIO CLI
- Přístup k portálu iDryer pro testování UIKit

---

## Krok 1. Větvení a klonování

1. Větvujte úložiště `idryer-core` na GitHubu.
2. Klonujte vaši vidličku místně:

    ```bash
    git clone https://github.com/<your-username>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. Ověřte, že smlouva projde ověřením v současném stavu:

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## Krok 2. Upravte smlouvu

Všechny změny jdou do `contracts/mqtt_contract.yaml`. Udržujte vše v jedné změně.

!!! varování
    Neupravujte soubory v `_generated/` — jsou přepsány generátory.

### 2a. Slovník schopností (nový typ periférie)

Pokud má zařízení nový hardwarový typ (např. senzor CO2), přidejte položku do sekce `capability_vocabulary`:

```yaml
capability_vocabulary:
  co2:
    description: "Senzor CO2 (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

To automaticky přidá pole `hasAirCo2: bool` do `iDryer::Config` při příští regeneraci.

### 2b. Kanonické role (nová role + widget)

Pokud zařízení vystavuje nový prvek nabídky, zaregistrujte roli v `canonical_roles`:

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

Hodnota `widget` je název React komponenty, kterou napíšete v kroku 5.

### 2c. Vyvolat akce (pokud widget posílá příkazy)

Pokud widget spustí akci na zařízení, popište ji v `invoke_actions`:

```yaml
invoke_actions:
  my_device:
    co2.calibrate:
      description: "Spustit kalibraci senzoru CO2"
      args:
        targetPpm:
          type: uint16
          description: "Referenční hodnota CO2 (ppm)"
          required: true
```

### 2d. Profil zařízení (nový typ zařízení)

Přidejte profil do `device_profiles`:

```yaml
device_profiles:
  my_device:
    description: "Moje zařízení"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

Hodnoty schopností pocházejí z `capability_vocabulary` definované v kroku 2a.

---

## Krok 3. Ověřte a regenerujte

```bash
cd contracts
./regen.sh
```

Příznaky:

| Příznak | Efekt |
|---|---|
| (žádný) | Ověř + všechny generátory + kopie na portal |
| `--firmware-only` | Pouze generátory firmwaru, přepusť kopii portálu |
| `--help` | Zobrazit nápovědu |

V úspěchu se aktualizuje `_generated/`:

- `uart_protocol.h`, `mqtt_topics.h` — hlavičky C++
- `iDryer_api.h` — Config/DeviceType fasáda
- `mqtt-api.types.ts` — TypeScript typy
- `scaffolds/my_device/` — PlatformIO kostry projektů
- Na portálu: soubory v `src/components/widgets/`

Pokud `regen.sh` skončí s chybou, problém opravte, než budete pokračovat.

---

## Krok 4. Implementujte firmware

Použijte generovaný projekt kostry:

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

Vyplňte sekce TODO v `src/main.cpp`:

- `onOnline()` — načtěte konfiguraci z NVS, inicializujte hardware.
- `loop()` — zjišťujte senzory, volajte `s_runtime.publishTelemetry(tel)`.
- `buildInfoJson()` — již naplněno generátorem ze schopností.
- `onInvoke()` — zpracujte `co2.calibrate`.

Podrobnosti viz [01-add-new-product.md](01-add-new-product.md).

---

## Krok 5. Vytvořte widget React

Widgety jsou v `contracts/widgets/` a jsou kopirovány na portál pomocí `regen.sh`.

!!! poznámka
    Neupravujte widgety přímo v `portal/src/components/widgets/` — budou přepsány při příštím `regen.sh`. Upravujte pouze v `contracts/widgets/`.

### Vytvořte soubor widgetu

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

### Zaregistrujte v index.ts

```ts
// contracts/widgets/index.ts
export { Co2DisplayWidget } from "./Co2Display";
```

### Zaregistrujte v widget-registry.tsx (na portálu)

Po příštím `regen.sh` se soubor zobrazí v `portal/src/components/widgets/Co2Display.tsx`. Ručně přidejte položku do `widget-registry.tsx`:

```tsx
import { Co2DisplayWidget } from "./Co2Display";

export const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  // ...
  Co2Display: Co2DisplayWidget,
};
```

---

## Krok 6. Testujte v UIKit

Otevřete `portal/src/pages/UiKitPage.tsx` a přidejte sekci s fiktivními daty uvnitř skupiny **Device Dashboard Widgets**:

```tsx
<KitSection title="Co2Display">
  <Co2DisplayWidget device={MOCK_DEVICE} item={MOCK_CO2_ITEM} socket={null} />
</KitSection>
```

Otevřete portál místně a přejděte na `/uikit` — widget by se měl vykreslit bez přihlášení.

---

## Krok 7. Kontrolní seznam PR

Před odesláním PR ověřte:

- [ ] `./contracts/regen.sh` se zahájit bez chyb
- [ ] `_generated/*` je potvrzen (nikoli v `.gitignore`)
- [ ] `contracts/widgets/` — přidán nový soubor widgetu
- [ ] `contracts/widgets/index.ts` — widget exportován
- [ ] `widget-registry.tsx` na portálu — widget zaregistrován
- [ ] Widget se vykreslí na `/uikit` bez chyb konzoly
- [ ] Kostra v `_generated/scaffolds/my_device/` správně odráží schopnosti
- [ ] Popis PR uvádí: účel zařízení, schopnosti, název widgetu

Odesláním PR proti větvi `main` úložiště `idryer-core`.

---

## Všechny změny v jednom PR

| Soubor | Typ změny |
|---|---|
| `contracts/mqtt_contract.yaml` | Zdroj pravdy |
| `contracts/_generated/*` | Auto-generováno — potvrzeno v plnosti |
| `contracts/widgets/MyWidget.tsx` | Nový soubor |
| `contracts/widgets/index.ts` | +1 řádek exportu |
| *(portal, po `regen.sh`)* | `src/components/widgets/MyWidget.tsx` — kopie |
| *(portal, ručně)* | `src/components/widgets/widget-registry.tsx` — +1 záznam |
| *(portal, ručně)* | `src/pages/UiKitPage.tsx` — +1 sekce v KitGroup |
