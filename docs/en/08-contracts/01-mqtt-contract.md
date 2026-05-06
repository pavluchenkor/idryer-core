# MQTT contract

The file `contracts/mqtt_contract.yaml` is the source of truth for the `idryer-core` MQTT interface.

## Scope

The contract describes **only what `MqttClient` from `idryer-core` implements**:

- topics the library can publish to
- commands the library accepts and routes

The full platform interface (all backend commands for all device types, including `drying`, `storage`, `profile`, `rfid`, etc.) is in `contracts/portal_backend_status.md` — this is the [Platform Reference].

## Device topics (device → backend)

| Suffix | Retained | Status |
|--------|----------|--------|
| `info` | yes | implemented |
| `telemetry` | no | implemented |
| `status` | yes | implemented |
| `config` | no | implemented |
| `config/delta` | no | implemented |
| `events` | no | implemented |
| `integrations/status` | yes | implemented |
| `offline` (LWT) | no | implemented |

## Commands (backend → device)

| Suffix | Handler | Status |
|--------|---------|--------|
| `commands/ping` | `IdryerRuntime` (built-in) | implemented |
| `commands/invoke` | product `CommandHandler` (rec.); fallback → `ActionDispatcher` | implemented |
| `commands/set` | product `CommandHandler` (rec.); fallback → `ActionDispatcher` | implemented |
| `commands/link_integration` | `LinkIntegrationsManager` via `CommandHandler` | implemented |
| `commands/bambu_apply` | `LinkIntegrationsManager` via `CommandHandler` | implemented |
| all others | product `CommandHandler` | product-defined |

## Change rule

Any change to the MQTT protocol in `idryer-core` must simultaneously touch:

1. `contracts/mqtt_contract.yaml`
2. Library code (`mqtt_client.h/.cpp`)
3. Portal / backend code

Update the contract first, then the code.

## Compatibility

- Adding new optional fields to a payload is safe.
- Renaming existing fields requires simultaneous updates to firmware, portal, and contract.
- `info` and `config` payloads are defined by the product — `idryer-core` does not validate them.
