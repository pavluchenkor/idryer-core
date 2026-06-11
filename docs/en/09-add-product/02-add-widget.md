# Add a Dashboard Card for a New Device

A "widget" in the iDryer ecosystem is a **device card on the portal dashboard** — a product-specific React component that composes the device's UI from reusable blocks. There is no widget registry, no generated React files, no `contracts/widgets/` directory.

This page covers adding such a card for a new device type. For firmware-only work see [01-add-new-product.md](01-add-new-product.md).

---

## What lives where

| Layer | Repository | Source of truth |
|---|---|---|
| Contract (deviceType, capabilities, invoke actions, telemetry, menu) | `idryer-core` | `contracts/mqtt_contract.yaml` |
| Generated headers / TS types / Dart types | `idryer-core` | `contracts/_generated/*` |
| Firmware | per device repo (e.g. `iHeater-link`, `iDryer-Storage`) | hand-written |
| Dashboard card | `iDryerPortal/frontend-v2` | `src/components/dashboard/cards/*.tsx` |
| Reusable card blocks | `iDryerPortal/frontend-v2` | `src/components/device/*.tsx` |
| Card registration | `iDryerPortal/frontend-v2` | `src/components/dashboard/DeviceDashboardCard.tsx` |
| UIKit preview | `iDryerPortal/frontend-v2` | `src/pages/UiKitPage.tsx` |

---

## Command channel rules

Cards talk to devices via two MQTT paths defined in the contract:

- **Invoke (form A)** — emit a menu action by its `id`, no args. Use when the action is already in the device menu and reachable from another client.
- **Invoke (form B)** — emit `{action, args}` directly, bypassing the menu. Use for parameterized actions (`heat.start` with `tempC`/`durationMin`, `led.pulse` with `r/g/b/animation`).
- **Set** — write a config value (`set <role> <value>`). Use only for persistent settings, not for processes with a beginning and an end.

For processes that start and finish (heating, drying, animation), always use **invoke**, never `set heat_active=true` style toggles.

---

## Telemetry null policy

If a sensor is missing or its reading is unavailable, the firmware must omit the field from the telemetry payload (not send `null`, not send `0`, not send `NaN`). The card must treat an absent field as "no data" and render a placeholder instead of guessing zero.

See `rules.telemetry_null_policy` in the contract for the canonical wording.

---

## Checklist — adding a card for a new device

1. **Contract** — add the device profile and any new capabilities, roles and invoke actions to `contracts/mqtt_contract.yaml`. Run `./regen.sh` and commit the regenerated `_generated/*`.
2. **Firmware** — implement `onCommand("invoke")` for the new actions; emit telemetry per the null policy.
3. **Card component** — create `iDryerPortal/frontend-v2/src/components/dashboard/cards/<DeviceType>Card.tsx`. Compose it from the reusable blocks in [src/components/device/](https://github.com/iDryer/iDryerPortal/tree/main/frontend-v2/src/components/device).
4. **Register** — add the new `deviceType` to the switch in [DeviceDashboardCard.tsx](https://github.com/iDryer/iDryerPortal/blob/main/frontend-v2/src/components/dashboard/DeviceDashboardCard.tsx).
5. **DeviceDetailPage** — extend `controlsOrProgress` in [DeviceDetailPage.tsx](https://github.com/iDryer/iDryerPortal/blob/main/frontend-v2/src/pages/DeviceDetailPage.tsx) so the same card appears on the device page.
6. **UIKit** — add an Idle + Active example to the "Device Widgets" group in [UiKitPage.tsx](https://github.com/iDryer/iDryerPortal/blob/main/frontend-v2/src/pages/UiKitPage.tsx) with mock data so the card can be inspected at `/uikit` without a real device.
7. **Test** — run the portal locally, verify the card renders correctly Idle and Active, sends the expected invoke payload, and reacts to telemetry updates.
8. **PR** — open one PR in `idryer-core` (contract + firmware submodule bumps) and one in `iDryerPortal` (card + registration + UIKit). Link them in the description.

---

## Reusable card blocks

These live in [src/components/device/](https://github.com/iDryer/iDryerPortal/tree/main/frontend-v2/src/components/device) and should be the first building blocks you reach for. Compose them before writing custom JSX.

| Block | Purpose |
|---|---|
| `DeviceHeader` | Device name, status pill, online/offline indicator |
| `DeviceTelemetryBlock` | Renders a list of telemetry rows, hides missing fields by default |
| `ActiveSessionBlock` | Progress UI for processes with target + remaining time |
| `NumberInput` | Numeric input bound to a min/max/step from the menu metadata |
| `CardActions` | Bottom-row button group (Start / Stop etc.) |

If a new block would be reused by ≥ 2 cards, add it under `src/components/device/` rather than inlining it.

---

## Existing cards (reference)

| Card | Device type | Notes |
|---|---|---|
| `HeaterCard` | `IHEATER_LINK` | Idle: temp + duration inputs + Start. Active: ActiveSessionBlock with remaining time + Stop. |
| `StorageCard` | `STORAGE_LINK` | SHT31 telemetry + LED animation/color picker + Turn On/Off (invoke `led.pulse`). |
| `IDryerCard` | fallback | Generic card for devices without a dedicated implementation. |

Open them as concrete examples before starting a new card.

---

## What used to exist and why it's gone

Earlier the project tried to keep widgets inside `idryer-core/contracts/widgets/`, ship them through `regen.sh` to the portal, and register them in a `widget-registry.tsx`. That layer was removed on **2026-05-27**: dashboard cards are product-specific React code, the contract has no opinion on JSX, and an extra copy-step between repositories added friction without value.

If you find references to the old approach (`widget-registry`, `contracts/widgets/`, `Co2DisplayWidget`-style names) in older docs or branches — they are obsolete. Use this page.
