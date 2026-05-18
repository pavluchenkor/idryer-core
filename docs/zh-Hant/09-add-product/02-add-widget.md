# 添加小部件和新設備

Complete cycle: from forking the repository to a merged PR. Covers firmware, contract, React widget, and portal testing.

If you only need firmware without a new widget — see [01-add-new-product.md](01-add-new-product.md).

---

## 先決條件

- Python 3.9+ with `pip install pyyaml jsonschema`
- Node.js 18+
- PlatformIO CLI
- Access to the iDryer portal for UIKit testing

---

## Step 1. Fork and Clone

1. Fork the `idryer-core` repository on GitHub.
2. Clone your fork locally:

    ```bash
    git clone https://github.com/<your-username>/idryer-core.git
    cd idryer-core
    git checkout -b feature/my-new-device
    ```

3. Verify the contract passes validation in the current state:

    ```bash
    cd contracts
    ./regen.sh --firmware-only
    ```

---

## Step 2. Edit the Contract

All changes go into `contracts/mqtt_contract.yaml`. Keep everything in a single changeset.

!!! warning
    Do not edit files in `_generated/` — they are overwritten by generators.

### 2a. Capability vocabulary (new peripheral type)

If the device has a new hardware type (e.g., a CO2 sensor), add an entry to the `capability_vocabulary` section:

```yaml
capability_vocabulary:
  co2:
    description: "CO2 sensor (ppm)"
    config_flag: hasAirCo2
    telemetry_field: airCo2Ppm
```

This automatically adds the field `hasAirCo2: bool` to `iDryer::Config` on the next regeneration.

### 2b. Canonical roles (new role + widget)

If the device exposes a new menu item, register the role in `canonical_roles`:

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

The `widget` value is the name of the React component you will write in Step 5.

### 2c. Invoke actions (if the widget sends commands)

If the widget triggers an action on the device, describe it in `invoke_actions`:

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

### 2d. Device profile (new device type)

Add the profile to `device_profiles`:

```yaml
device_profiles:
  my_device:
    description: "My device"
    capabilities: [led, co2]
    invoke_actions: [co2.calibrate]
```

Capability values come from the `capability_vocabulary` defined in step 2a.

---

## Step 3. Validate and Regenerate

```bash
cd contracts
./regen.sh
```

Flags:

| Flag | Effect |
|---|---|
| (none) | Validate + all generators + copy to portal |
| `--firmware-only` | Firmware generators only, skip portal copy |
| `--help` | Show help |

On success, `_generated/` is updated with:

- `uart_protocol.h`, `mqtt_topics.h` — C++ headers
- `iDryer_api.h` — Config/DeviceType facade
- `mqtt-api.types.ts` — TypeScript types
- `scaffolds/my_device/` — PlatformIO project skeleton
- On the portal: files in `src/components/widgets/`

If `regen.sh` exits with an error, fix the problem before continuing.

---

## Step 4. Implement Firmware

Use the generated scaffold project:

```bash
cp -r contracts/_generated/scaffolds/my_device/ ~/my_device_fw/
cd ~/my_device_fw
```

Fill in the TODO sections in `src/main.cpp`:

- `onOnline()` — load config from NVS, initialize hardware.
- `loop()` — poll sensors, call `s_runtime.publishTelemetry(tel)`.
- `buildInfoJson()` — already populated by the generator from capabilities.
- `onInvoke()` — handle `co2.calibrate`.

For details, see [01-add-new-product.md](01-add-new-product.md).

---

## Step 5. Create the React Widget

Widgets live in `contracts/widgets/` and are copied to the portal by `regen.sh`.

!!! note
    Do not edit widgets directly in `portal/src/components/widgets/` — they will be overwritten on the next `regen.sh` run. Edit only in `contracts/widgets/`.

### Create the widget file

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

### Register in index.ts

```ts
// contracts/widgets/index.ts
export { Co2DisplayWidget } from "./Co2Display";
```

### Register in widget-registry.tsx (on the portal)

After the next `regen.sh` run the file will appear at `portal/src/components/widgets/Co2Display.tsx`. Add an entry to `widget-registry.tsx` manually:

```tsx
import { Co2DisplayWidget } from "./Co2Display";

export const WIDGET_REGISTRY: Record<WidgetName, React.ComponentType<WidgetProps>> = {
  // ...
  Co2Display: Co2DisplayWidget,
};
```

---

## Step 6. Test in UIKit

Open `portal/src/pages/UiKitPage.tsx` and add a section with mock data inside the **Device Dashboard Widgets** group:

```tsx
<KitSection title="Co2Display">
  <Co2DisplayWidget device={MOCK_DEVICE} item={MOCK_CO2_ITEM} socket={null} />
</KitSection>
```

Open the portal locally and navigate to `/uikit` — the widget should render without a login.

---

## Step 7. PR Checklist

Before submitting the PR, verify that:

- [ ] `./contracts/regen.sh` completes without errors
- [ ] `_generated/*` is committed (not in `.gitignore`)
- [ ] `contracts/widgets/` — new widget file added
- [ ] `contracts/widgets/index.ts` — widget exported
- [ ] `widget-registry.tsx` on the portal — widget registered
- [ ] Widget renders at `/uikit` without console errors
- [ ] Scaffold in `_generated/scaffolds/my_device/` correctly reflects capabilities
- [ ] PR description states: device purpose, capabilities, widget name

Submit the PR against the `main` branch of the `idryer-core` repository.

---

## 一個 PR 中的所有更改

| File | Change type |
|---|---|
| `contracts/mqtt_contract.yaml` | Source of truth |
| `contracts/_generated/*` | Auto-generated — committed in full |
| `contracts/widgets/MyWidget.tsx` | New file |
| `contracts/widgets/index.ts` | +1 export line |
| *(portal, after `regen.sh`)* | `src/components/widgets/MyWidget.tsx` — copy |
| *(portal, manual)* | `src/components/widgets/widget-registry.tsx` — +1 entry |
| *(portal, manual)* | `src/pages/UiKitPage.tsx` — +1 section in KitGroup |
