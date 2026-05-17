# Hranice: knihovna a produkt

## Co je v knihovně

Knihovna (\`lib/idryer-core/\`) obsahuje:

- Celý stack sítě: WiFi, HTTP, MQTT, TLS.
- Protokol provisioning/claiming.
- Stavový stroj cloudu (\`CloudStateMachine\`).
- UART most a frame protokol.
- Integrační klienty (Bambu, HA, Moonraker).
- Rozhraní zařízení (\`IWifiManager\`, \`ICredentialStore\`, \`IHttpClient\`, \`IProfile\`).
- Arduino implementace těchto rozhraní.
- MQTT témata a logika publish/subscribe.

Test na kód patřící do knihovny: **jakýkoli produkt s jakýmkoli hardwarem jej může používat bez modifikace**.

## Co je v produktu

Produkt (\`src/\`) obsahuje:

- Implementaci \`IProfile\` — konfiguraci, informační zátěž, \`applyConfig\`.
- Obchodní logiku specifickou pro zařízení (LED kontrola, sušení, topení).
- Obsluhy \`onInvoke\` / \`onSetCommand\`.
- Senzory produktu a publikování telemetrie.
- Inicializaci periférie (FastLED, Wire, ImprovWiFi).
- Kořen kompozice v \`main.cpp\`.

Test na kód patřící do produktu: **bez změny hardwaru nebo konfigurace je to bezvýznamné**.

## Konkrétní příklady

| Kód | Kde žije | Proč |
|-----|----------|------|
| \`MqttClient\` | knihovna | každý produkt potřebuje MQTT |
| \`CloudStateMachine\` | knihovna | provisioning/claiming je stejné pro všechny |
| \`ArduinoWifiManager\` | knihovna | WiFi připojení nezávisí na produktu |
| \`LedStripProfile\` | produkt | specifické pro Storage Link |
| \`LedStripExecutor\` | produkt | řídí FastLED, není potřeba pro jiná zařízení |
| \`Sht31ClimateSensor\` | produkt | specifický senzor pro konkrétní produkt |
| \`StorageTelemetryPublisher\` | produkt | zná formát telemetrie Storage Link |
| \`IProfile\` | knihovna | smlouva, kterou knihovna volá |
| \`BambuClient\` | knihovna | integrace je znovu použita přes iDryer a iHeater |

## Rozhraní jako hranice

Knihovna zná produkt pouze přes \`IProfile\`. Veškerá interakce probíhá přes pět metod:

\`\`\`cpp
profile->onOnline();               // knihovna → produkt: poprvé online
profile->loop();                   // knihovna → produkt: každý cyklus
profile->buildInfoJson(buf, len);  // knihovna → produkt: potřeba informační zátěž
profile->getConfig(doc);           // knihovna → produkt: potřeba konfigurace
profile->applyConfig(id, val);     // knihovna → produkt: přijat set příkaz
\`\`\`

Produkt zná knihovnu přes \`MqttClient\` (pro publikování telemetrie/events) a přes zpětná volání \`ActionDispatcher\` (pro příkazy).

## Co nesmí překročit hranici

- Knihovna nesmí zahrnovat hlavičky produktu.
- Produkt nesmí volat \`CloudStateMachine::handleProvisioning()\` nebo jiné privátní metody stack přímo — jen skrze veřejné API.
- Telemetrie produktu se publikuje přímo skrze \`s_mqtt.publishTelemetry()\` — runtime ji nevidí.
