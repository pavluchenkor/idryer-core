# MQTT smlouva

Soubor `contracts/mqtt_contract.yaml` je zdrojem pravdy pro rozhraní MQTT knihovny `idryer-core`.

## Rozsah

Smlouva popisuje **pouze to, co implementuje `MqttClient` z `idryer-core`**:

- témata, která může knihovna publikovat
- příkazy, které knihovna přijímá a směruje

Úplné rozhraní platformy (všechny příkazy backendu pro všechny typy zařízení, včetně `drying`, `storage`, `profile`, `rfid` atd.) je v `contracts/portal_backend_status.md` — to je [Platform Reference].

## Témata zařízení (zařízení → backend)

| Přípona | Zachován | Stav |
|--------|----------|--------|
| `info` | ano | implementováno |
| `telemetry` | ne | implementováno |
| `status` | ano | implementováno |
| `config` | ne | implementováno |
| `config/delta` | ne | implementováno |
| `events` | ne | implementováno |
| `integrations/status` | ano | implementováno |
| `offline` (LWT) | ne | implementováno |

## Příkazy (backend → zařízení)

| Přípona | Obslužný program | Stav |
|--------|---------|--------|
| `commands/ping` | `IdryerRuntime` (vestavěný) | implementováno |
| `commands/invoke` | Obsluha příkazů produktu (doporučeno); fallback → `ActionDispatcher` | implementováno |
| `commands/set` | Obsluha příkazů produktu (doporučeno); fallback → `ActionDispatcher` | implementováno |
| `commands/link_integration` | `LinkIntegrationsManager` prostřednictvím `CommandHandler` | implementováno |
| `commands/bambu_apply` | `LinkIntegrationsManager` prostřednictvím `CommandHandler` | implementováno |
| vše ostatní | Obsluha příkazů produktu | definováno produktem |

## Pravidlo změny

Jakákoliv změna protokolu MQTT v `idryer-core` musí současně dotknout:

1. `contracts/mqtt_contract.yaml`
2. Kód knihovny (`mqtt_client.h/.cpp`)
3. Kód portálu / backendu

Nejdříve aktualizujte smlouvu, poté kód.

## Kompatibilita

- Přidání nových volitelných polí do datové části je bezpečné.
- Přejmenování existujících polí vyžaduje současné aktualizace firmwaru, portálu a smlouvy.
- Datové části `info` a `config` jsou definovány produktem — `idryer-core` je nevaliduje.
