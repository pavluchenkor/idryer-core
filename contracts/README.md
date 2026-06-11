# iDryer Contracts

Single source of truth for all iDryer platform communication channels:
MQTT (portal/HA/Bambu), UART (RP2040<->ESP), WebSocket (Moonraker, local-WS),
HTTP REST (portal claim flow), HA discovery, WiFi provisioning.

## If You Are New Here

```bash
# 1. Show contract map:
python3 contracts/show.py

# 2. Find something specific (example: storage led.pulse):
python3 contracts/show.py invoke_actions.storage_link.led.pulse

# 3. List all actions (across all products):
python3 contracts/show.py --actions

# 4. After editing YAML, always run:
./contracts/regen.sh        # validate + regenerate all _generated/*
```

Do not edit files in `_generated/` manually. They are overwritten by generators.

## Structure

```text
mqtt_contract.yaml          <- source of truth (yaml)
mqtt_contract.schema.json   <- meta-schema (JSON Schema, validates yaml)

show.py                     <- navigator: dotted-path queries + JSON examples
validate_contract.py        <- yaml validation + cross-refs + sizeof checks
gen_idryer_api_h.py         <- yaml -> C++ facade enums/structs (iDryer_api.h)
gen_uart_protocol_h.py      <- yaml -> C++ UART header
gen_mqtt_topics_h.py        <- yaml -> C++ MQTT topics header
gen_ts_types.py             <- yaml -> TypeScript types

regen.sh                    <- single entry point: validate + regenerate all
pre_commit.sh               <- git hook: runs regen.sh + sync-check
HOOKS.md                    <- hook setup instructions

_generated/                 <- generator outputs (DO NOT EDIT)
  uart_protocol.h           <- C++ structs / enums / kind ids
  mqtt_topics.h             <- C++ topic constants / QoS / retained
  mqtt-api.types.ts         <- TS types for portal
```

## Navigation (`show.py`)

The contract is large (~3000 YAML lines). `show.py` extracts specific sections
by dotted path, highlights output in terminal, and prints ready JSON examples
for `mosquitto_pub -m '...'`.

```bash
# File map + top-level sections
python3 contracts/show.py

# Names only (no content)
python3 contracts/show.py --list                     # top-level
python3 contracts/show.py --list invoke_actions      # section children

# Node content
python3 contracts/show.py invoke_actions                          # full section
python3 contracts/show.py invoke_actions.storage_link             # product subsection
python3 contracts/show.py invoke_actions.storage_link.led.pulse   # single action
                                                                  # (dots in names are supported)

# Flat list of all invoke actions across products
python3 contracts/show.py --actions

# Specific enum / payload / message
python3 contracts/show.py enums.UartDeviceType
python3 contracts/show.py payloads.Telemetry
python3 contracts/show.py messages.command_drying
```

Options:
- `--no-color` - disable ANSI highlighting.
- `--no-examples` - do not append JSON examples.

Useful alias (one-time in `~/.zshrc`):
```bash
alias contract='python3 contracts/show.py'
# then: contract invoke_actions.storage_link.led.pulse
```

## Add a New Device

Full workflow (fork -> yaml -> regen -> firmware -> dashboard card -> UIKit -> PR):

-> **[docs/en/09-add-product/02-add-widget.md](../docs/en/09-add-product/02-add-widget.md)** (source of truth)
-> **[docs/ru/09-add-product/02-add-widget.md](../docs/ru/09-add-product/02-add-widget.md)** (outdated, see EN)

A "widget" in this codebase is a **device card on the portal dashboard** — a product-specific
React component in `iDryerPortal/frontend-v2/src/components/dashboard/cards/`. The contract
has no opinion on JSX; there is no widget registry and no generated React files.

Dashboard cards currently in the portal:
- `HeaterCard` — `IHEATER_LINK`
- `StorageCard` — `STORAGE_LINK`
- `IDryerCard` — fallback for devices without a dedicated card

Adding a card for a new device type is a portal-side task (see the page linked above).

Short flow:

```text
mqtt_contract.yaml
  capability_vocabulary   <- new peripheral -> hasXxx in Config
  canonical_roles         <- semantic role for menu items
  invoke_actions          <- command arguments
  device_profiles         <- device capabilities + invoke_actions
        |
        +-> ./contracts/regen.sh
              +-> _generated/scaffolds/my_device/  (firmware scaffold)
              +-> mqtt-api.types.ts                (TS types -> portal)
              +-> roles.*.json                     (i18n -> portal)
              +-> canonical_roles.dart             (mobile)
```

The portal then consumes `mqtt-api.types.ts` and renders the device with the appropriate card
(switched by `deviceType` in `src/components/dashboard/DeviceDashboardCard.tsx`).

## Pipeline

```bash
./contracts/regen.sh
```

Internally: `validate_contract.py` -> all generators in sequence.
Generator lists are kept in `FIRMWARE_GENERATORS` / `ALL_GENERATORS` in `regen.sh`.

`pre_commit.sh` runs the same `regen.sh` and checks that `_generated/*`
stays in sync with repository content (see `HOOKS.md`).

## Change Rule

Any communication-channel change should update, in one changeset:

1. `mqtt_contract.yaml`
2. regenerated `_generated/*`
3. firmware / portal code

Order is strict: contract first, code second.
Pre-commit hook prevents committing YAML with outdated `_generated/*`.

## YAML Coverage

| Channel | Section |
|---|---|
| MQTT (portal) | `messages`, `mqtt_only`, `legacy_command_topics` |
| UART (RP2040<->ESP) | `messages.bindings.uart`, `uart_only`, `uart_kind_ranges` |
| HA integration runtime | `ha_integration_topics` |
| HA Discovery | `ha_discovery_topics` |
| Bambu LAN MQTT | `bambu_lan_mqtt` |
| Moonraker WebSocket | `moonraker_websocket` |
| Local WebSocket server | `local_websocket` |
| Cloud HTTP API (claim) | `cloud_http_api` |
| WiFi provisioning | `wifi_provisioning` |
