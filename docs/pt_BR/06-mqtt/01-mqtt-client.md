# MqttClient

\`MqttClient\` je MQTT klient zařízení. Obaluje \`PubSubClient\`, spravuje připojení a směruje příchozí zprávy. Všechna témata se tvoří automaticky ze sériového čísla zařízení.

## Inicializace

\`\`\`cpp
void MqttClient::begin(const char* serialNumber, const char* token);
\`\`\`

Voláno \`CloudStateMachine\` po úspěšném provisioningu. Nespojuje se okamžitě — nastavuje parametry a konfiguruje TLS.

Parametry:

- \`serialNumber\` — sériové číslo zařízení. Používáno jako ID MQTT klienta a jméno uživatele.
- \`token\` — token zařízení. Používáno jako heslo MQTT.

Při kompilaci s příznakem \`MQTT_USE_TLS=1\` klient konfiguruje \`WiFiClientSecure\` s Let's Encrypt root CA (vložené v \`root_ca.h\`).

\`\`\`cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // viz "Buffer size" níže
mqttClient_.setKeepAlive(60);
\`\`\`

## Velikost bufferu {#buffer-size}

\`PubSubClient\` standardně používá 256-bajtový buffer — dostatečný jen pro krátké zprávy. Pro zařízení iDryer je to příliš malé: hlavní "těžkou" zátěž tvoří konfigurace zařízení (nabídka), která se publikuje do tématu \`idryer/{serial}/config\` najednou.

\`MqttClient\` nastavuje buffer na \`MQTT_BUFFER_SIZE\` a omezuje velikost kusu pro velké konfigurace na \`MQTT_CONFIG_CHUNK_SIZE\`. Obě konstanty jsou definovány v \`lib/idryer-core/src/mqtt/mqtt_client.h\`:

\`\`\`cpp
#define MQTT_BUFFER_SIZE        16384  // PubSubClient buffer
#define MQTT_CONFIG_CHUNK_SIZE  16000  // maximální data v jednom chunku konfigurace
\`\`\`

Vztah mezi nimi:

- \`MQTT_BUFFER_SIZE\` (16384 bajtů) — horní limit pro **jednu MQTT zprávu**. Jakýkoli \`publish*()\` call s zátěží větší než toto bude vynechán \`PubSubClient\` bez odeslání.
- \`MQTT_CONFIG_CHUNK_SIZE\` (16000 bajtů) — maximální velikost \`"d"\` (datové části) uvnitř jednoho \`publishConfigRaw\` chunku. 384-bajtová rezerva je vyhrazena pro obálku chungu: \`{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}\` plus automaticky přidané pole \`timestamp\`.

### Proč 16384

Číslo bylo zvoleno ne pro estetiku ale ze **maximálního očekávaného payloadu zařízení**, kterým je přenos nastavení/nabídky:

- Konfigurace Storage Link a Link/iHeater (nabídka) se serializuje jako JSON s escapingem. Úplný snímek aktuální nabídky se vejde asi do 10–14 KB.
- Rezerva na 16384 pokrývá růst nabídky bez nutnosti dělení do chunků.
- Hodnota je násobkem 4 KB — vhodné pro alokaci na ESP32.

Pokud tvůj produkt má větší konfiguraci (např. rozšířenou nabídku s mnoha položkami nebo binárními hodnotami), jsou dostupné dvě cesty:

1. **Zvýšit \`MQTT_BUFFER_SIZE\`** — přepsat přes \`build_flags\` v \`platformio.ini\`:
   \`\`\`ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   \`\`\`
   Pamatuj si na RAM: \`PubSubClient\` drží tento buffer nepřetržitě. Na ESP32-C3 (~400 KB volné heap) je 32 KB přijatelné, ale jít dál nese rizika.

2. **Využít \`publishConfigRaw(json, length)\`** — rozděluje zátěž na chunky \`MQTT_CONFIG_CHUNK_SIZE\`; backend je znovu spojuje podle polí \`tid\` / \`idx\` / \`total\` / \`last\`. Tato cesta je preferována pro konfigurace přicházející z RP2040 přes UART v kusech libovolné délky.

### Platí pro publikace produktu

Stejný 16384-bajtový strop se aplikuje na \`publishTelemetry\`, \`publishStatus\`, \`publishEvent\`. V praxi jsou telemetrie a eventy mnohem menší (stovky bajtů); jen publikace konfigurací se blíží tomuto limitu. Pokud tvůj projekt periodicky publikuje velkou zátěž (např. dump pole měření), odhadni její velikost předem nebo si ji sám rozděl.

## Připojení

\`\`\`cpp
bool MqttClient::connect();
\`\`\`

Provádí:

1. Připojení k brokerovi s trvalou relací (\`clean_session = false\`). Trvalá relace je povinná — bez ní jsou ztraceny příkazy přicházející když je zařízení offline.
2. Nastavuje LWT zprávu v tématu \`idryer/{serial}/offline\` (QoS 1, není zachováno).
3. Přihlašuje se k \`idryer/{serial}/commands/#\` (QoS 1). Dělá až 3 pokusy; při selhání se odpojí.

Vrací \`true\` pokud se připojení a přihlášení podařilo.

## Loop

\`\`\`cpp
void MqttClient::loop();
\`\`\`

Volá se každou iteraci. Znovu se připojí při odpojení, pak volá \`PubSubClient::loop()\` pro příjem příchozích zpráv.

## Publikování

Všechny metody publikace přidají pole \`timestamp\` (ISO 8601 UTC) pokud již v dokumentu není přítomno.

| Metoda | Téma | Zachováno |
|--------|------|-----------|
| \`publishInfoJson(const char* json)\` | \`idryer/{serial}/info\` | ano |
| \`publishTelemetry(JsonDocument&)\` | \`idryer/{serial}/telemetry\` | ne |
| \`publishStatus(JsonDocument&)\` | \`idryer/{serial}/status\` | ano |
| \`publishConfig(JsonDocument&)\` | \`idryer/{serial}/config\` | ne |
| \`publishEvent(JsonDocument&)\` | \`idryer/{serial}/events\` | ne |
| \`publishIntegrationsStatus(JsonDocument&)\` | \`idryer/{serial}/integrations/status\` | ano |
| \`publishConfigRaw(const char* json, size_t len)\` | \`idryer/{serial}/config\` | ne |
| \`publishConfigDelta(const char* json, size_t len)\` | \`idryer/{serial}/config/delta\` | ne |

\`publishConfigRaw\` automaticky rozděluje zátěž do chunků pokud velikost překročí \`MQTT_CONFIG_CHUNK_SIZE\` (16000 bajtů). Každý chunk obsahuje pole \`tid\`, \`idx\`, \`total\`, \`last\`, \`d\`.

!!! poznámka
    \`PubSubClient\` vždy publikuje na QoS 0, bez ohledu na nastavení tématu. Toto je omezení knihovny.

## Příjímání příkazů

Příchozí zprávy v tématu \`idryer/{serial}/commands/{cmd}\` se parsují jako JSON a předávají registrovanému \`CommandCallback\`:

\`\`\`cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
\`\`\`

Část \`{cmd}\` se extrahuje z tématu a předává se jako první argument. \`IdryerRuntime\` registruje tento callback v \`begin()\`.

## Pomocné metody

\`\`\`cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bajtů
static char* generateUuid(char* buffer);    // buffer >= 37 bajtů
\`\`\`

\`generateUuid\` generuje UUID v4 na základě \`esp_random()\`.

## Omezení

- Jedna instance \`MqttClient\` na zařízení (singleton přes \`instance_\`).
- Maximální velikost jedné JSON zprávy — \`MQTT_BUFFER_SIZE\` (výchozí 16384 bajtů). Dimenzionován pro nejzávažnější zátěž zařízení — typicky serializovanou konfiguraci (nabídku). Pro větší konfigurace zvýšit konstantu přes \`build_flags\` nebo použít \`publishConfigRaw\` s automatickým dělením chunků. Viz [Velikost bufferu](#buffer-size).
- TLS je povoleno příznakem budování \`MQTT_USE_TLS\`.
