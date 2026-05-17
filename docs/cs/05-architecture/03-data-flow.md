# Tok dat

Popis jak se data pohybují uvnitř běžícího zařízení. Cílem je ukázat, že \`idryer-core\` používá ani event bus ani service locator: účastníci jsou spojeni explicitními ukazateli v kořeni kompozice a každý směr dat je samostatná čitelná cesta.

Detailní návody na "jak směrovat data mezi mými částmi" jsou v [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md).

## Hlavní směry

\`\`\`
                Backend / app
                     │
                     │ MQTT commands/*
                     ▼
        ┌──────────────────────────────┐
        │  MqttClient                  │
        │  parsuje topic + payload     │
        └──────────────┬───────────────┘
                       │
                       │ CommandCallback
                       ▼
        ┌──────────────────────────────┐
        │  IdryerRuntime               │
        │  ping → settimeofday + info  │
        │  ostatní → CommandHandler    │
        └──────────────┬───────────────┘
                       │
                       │ commandHandler_(cmd, data)
                       ▼
        ┌──────────────────────────────┐
        │  Produkt handleCommand()     │
        │  invoke / set / get_config / │
        │  príkazy specifické produktu │
        └──────┬───────────────┬───────┘
               │               │
               ▼               ▼
   ActionDispatcher        IProfile
   handleInvoke / Set      getConfig
                           applyConfig
                           buildInfoJson
\`\`\`

\`\`\`
       Senzor (produkt)        Profil / executor
            │                           │
            │ tick() / read             │ aktualizuje stav
            ▼                           ▼
       ┌───────────────────────────────────────┐
       │  Vydavatel produktu                   │
       │  (StorageTelemetryPublisher, …)       │
       │  buduje JsonDocument                  │
       └────────────────┬──────────────────────┘
                        │
                        │ pub.publishX(doc)
                        ▼
       ┌───────────────────────────────────────┐
       │  DevicePublisher (volitelný)          │
       │  pomocník duální publikace: MQTT + WS│
       └─────────┬─────────────────────┬───────┘
                 │                     │
                 ▼                     ▼
            MqttClient            LocalAccess (WS)
            broker                LAN klient
\`\`\`

## Příchozí příkazy

1. **MQTT** doručí zprávu v tématu \`idryer/{serial}/commands/{cmd}\`.
2. \`MqttClient::handleMessage\` parsuje zátěž jako JSON a volá \`CommandCallback\`.
3. \`CommandCallback\` je registrován \`IdryerRuntime\` v \`begin()\` — přijímá \`(command, data)\`, kde \`command\` je přípona za \`commands/\`.
4. \`IdryerRuntime::onMqttCommand\`:
   - Pokud \`command == "ping"\` — synchronizuj čas a publikuj info. Není předáno dál.
   - Pokud je zaregistrován \`commandHandler_\` — předej všechno ostatní produktu.
   - Jinak — fallback vestavěná cesta: \`invoke\` → \`ActionDispatcher\`, \`set\` → \`ActionDispatcher\`, \`device.getConfig\` → \`IProfile::getConfig\`.

5. **Místní WS** (pokud se používá) přijímá \`{"type":"command","command":"...","data":{...}}\`, rozbalí obálku a volá stejný \`CommandSink\` zaregistrovaný pro MQTT cestu. Jeden handler — dva transporty.

## Odchozí data

Knihovna nic nepublikuje, pokud není požádána. Všechny odchozí zprávy jsou iniciovány produktem:

| Co | Iniciováno | Přes které API |
|----|-----------|---------------|
| \`info\` | \`IdryerRuntime\` (jednou když Online a na \`ping\`) | \`MqttClient::publishInfoJson\` |
| \`telemetry\` | vydavatel produktu | \`MqttClient::publishTelemetry\` nebo \`DevicePublisher::publishTelemetry\` |
| \`status\` | kód produktu na změně stavu | \`MqttClient::publishStatus\` nebo \`DevicePublisher::publishStatus\` |
| \`config\` | \`handleCommand\` na \`device.getConfig\` nebo \`get_config\` | \`MqttClient::publishConfig\` |
| \`events\` | kód produktu na event | \`MqttClient::publishEvent\` |
| \`integrations/status\` | \`LinkIntegrationsManager\` | \`MqttClient::publishIntegrationsStatus\` |
| \`offline\` | broker automaticky (LWT) | zařízení toto nikdy nepublikuje |

## Připojení objektů v kořeni kompozice

Reference mezi účastníky se předávají explicitně skrze konstruktory a settery. Žádné globální registry.

\`\`\`
ArduinoWifiManager     ─┐
ArduinoCredentialStore ─┤
HttpApi (← Http)       ─┼──→ CloudStateMachine ──→ IdryerRuntime ──→ MqttClient
MqttClient             ─┘                              ▲
                                                       │
                                ActionDispatcher ──────┤
                                IProfile         ──────┘

                LocalAccess  ──── (setCommandSink) ────→ stejný handleCommand
                DevicePublisher (&MqttClient, &LocalAccess)

                Senzor  ──→ Vydavatel  ──→ DevicePublisher  ──→ MqttClient + LocalAccess
                Executor ←── ActionDispatcher (invoke)  ←── handleCommand
\`\`\`

Každé připojení je jeden řádek v \`main.cpp\`. Toto je "explicitní kořen kompozice".

## Proč tento design

- **Bez kouzel**: aby čtenář pochopil jak data cestují ze senzoru do cloudu, vidí řetězec ukazatelů v \`main.cpp\`. Žádný tok dat není skryt za fasádou.
- **Flexibilita**: produkt si volí zda použit \`DevicePublisher\` (MQTT + WS), publikovat jen do MQTT nebo používat vlastní vydavatele s dodatečnou logikou.
- **Testovatelnost**: každý uzel je samostatná třída s explicitními závislostmi. Uzly lze nahradit mocky bez změny zbytku stacku.

## Co je záměrně nepřítomno

- Žádný globální event bus nebo message broker uvnitř zařízení.
- Žádná automatická detekce "mám senzor, budu jeho data publikovat sám".
- Žádný typ registru "zařízení zná všechny své poskytovatele telemetrie".

Pokud produkt potřebuje taková spojení — produkt je přidá do svého vlastního kódu. Knihovna je neukládá.
