# API pública: iDryer::Link

`iDryer::Link` é o único ponto de entrada para o programador do sistema embarcado. A fachada esconde toda a pilha SDK: WiFi/Improv, máquina de estados da nuvem, claim HTTP, MQTT, WebSocket local, NVS. O produto apenas precisa preencher os campos `telemetry`/`status`, registar retornos de chamada e invocar `begin()`/`loop()`.

---

## Ciclo de vida

Esqueleto típico de `main.cpp`:

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // necessário apenas se setCommandHandler for utilizado

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — estritamente DEPOIS de begin(): begin() instala o seu próprio despachante
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## Configuração: `iDryer::Config`

Preenchida uma única vez em `main.cpp`, passada ao construtor de `Link`. Todos os campos utilizam inicialização agregada (inicializadores designados em C++).

| Campo | Tipo | Fim | Nota |
|-------|------|-----|------|
| `deviceType` | `DeviceType` | Tipo de dispositivo | **obrigatório** |
| `unitsCount` | `uint8_t` | Número de unidades (câmaras), 1..`MAX_UNITS` (4) | **obrigatório** |
| `hasAirTemp` | `bool` | Sensor de temperatura do ar presente | false = campo omitido de JSON |
| `hasAirHumidity` | `bool` | Sensor de humidade presente | false = campo omitido de JSON |
| `hasHeaterTemp` | `bool` | Sensor de temperatura do aquecedor presente | — |
| `hasHeaterPower` | `bool` | Sensor de potência do aquecedor presente | — |
| `hasFanStatus` | `bool` | Estado da ventoinha presente | — |
| `hasScales` | `bool` | Balança presente | — |
| `hasRfid` | `bool` | Leitor RFID presente | — |
| `allowHa` | `bool` | Permitir integração Home Assistant | false = SDK não cria cliente |
| `allowBambu` | `bool` | Permitir integração Bambu Lab LAN | — |
| `allowMoonraker` | `bool` | Permitir integração Moonraker/Klipper | — |
| `telemetryPeriodMs` | `uint32_t` | Período de publicação automática de `Telemetry` (ms) | 0 = não publicar |
| `statusPeriodMs` | `uint32_t` | Período de publicação automática de `Status` (ms) | 0 = não publicar |
| `hardwareVersion` | `const char*` | Cadeia de versão de hardware | **obrigatório** |
| `firmwareVersion` | `const char*` | Cadeia de versão de firmware | **obrigatório** |

---

## Classe `iDryer::Link`

### Construtor

```cpp
explicit Link(const Config& cfg);
```

Toma a configuração por referência const. `CFG` deve existir durante toda a vida do objeto (tipicamente `static const`).

### Métodos

#### `begin()`

```cpp
bool begin();
```

Inicia toda a pilha SDK: WiFi/Improv, máquina de estados da nuvem, claim HTTP, MQTT, WebSocket local, persistência NVS.

Invocar uma única vez em `setup()`. Retorna `true` na inicialização bem-sucedida.

```cpp
void setup() {
    link.begin();
}
```

#### `loop()`

```cpp
void loop();
```

O único tick necessário. Atende WiFi/MQTT/LocalAccess e publica automaticamente telemetria e estado nos seus temporizadores.

Invocar em cada iteração de `loop()`. Sem esta invocação a conexão não é mantida.

```cpp
void loop() {
    link.loop();  // primeiro em loop(), antes da lógica do produto
}
```

*Fonte: `iDryer-Storage/src/main.cpp:253`, `iHeater-link/src/main.cpp:381`.*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

Publica imediatamente o estado atual de `link.telemetry`, independentemente do temporizador `telemetryPeriodMs`.

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

Publica imediatamente o estado atual de `link.status`. Utilizar após processar um comando quando o novo estado tem de ser refletido no portal imediatamente.

```cpp
// iHeater-link/src/main.cpp:238
device().publishStatusNow();
```

#### `raiseEvent()`

```cpp
void raiseEvent(EventKind   severity,
                const char* event,
                const char* message,
                uint8_t     unitId = 0xFF);
```

Publica um evento para o tópico `idryer/{serial}/events`. Enviado imediatamente.

| Parâmetro | Tipo | Fim |
|-----------|------|-----|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | Código de evento, p.ex. `"OVERHEAT"`, `"SESSION_COMPLETE"` |
| `message` | `const char*` | Texto de depuração arbitrário |
| `unitId` | `uint8_t` | Índice de unidade (0..unitsCount-1) ou `0xFF` para dispositivo |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

Registra um retorno de chamada para comandos comerciais (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) chegando sobre MQTT ou Local WS. A fonte do comando é transparente.

`RequestCallback` = `std::function<void(const iDryer::Request&)>`

```cpp
link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start: myStart(r.unitId, r.targetTempC); break;
        case iDryer::RequestKind::Stop:  myStop(r.unitId);                 break;
        default: break;
    }
});
```

**Importante:** se `runtime()->setCommandHandler(...)` estiver definido, este retorno de chamada não é invocado — o despachante completo interceta todos os comandos.

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

Registra um retorno de chamada para `commands/profile` — um programa de secagem em múltiplas etapas.

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

Invocado quando o estado de conexão de uma integração muda (HA, Bambu, Moonraker). Retorno de chamada opcional.

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

Invocado quando o fluxo de claim da nuvem retorna um PIN para entrada no portal.

`ClaimPinCallback` = `std::function<void(const char* pin, uint32_t expiresInSeconds)>`

```cpp
// iHeater-link/src/main.cpp:367
device().onClaimPin([](const char* pin, uint32_t expiresInSeconds) {
    Serial.printf("CLAIM_PIN:%s:%u\n", pin, expiresInSeconds);
});
```

#### `isOnline()`

```cpp
bool isOnline() const;
```

Retorna `true` se o dispositivo está registado e a sessão MQTT está ativa.

```cpp
// iHeater-link/src/main.cpp:281
if (device().isOnline()) { ... }
```

#### `serial()`

```cpp
const char* serial() const;
```

Número de série do dispositivo (cadeia de NVS, atribuído durante claim). Cadeia vazia antes do claim ser concluído.

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

Escreve credenciais WiFi em NVS apenas se ainda não estiverem definidas. Invocar antes de `begin()`. Utilizado em ambientes de desenvolvimento com credenciais codificadas.

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

Sempre sobrescreve credenciais WiFi em NVS. Auxiliar de desenvolvimento e re-aprovisionamento forçado.

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

Inicia manualmente o fluxo de claim da nuvem (provision → register → check-claim). Em caso de sucesso invoca o retorno de chamada registado `onClaimPin`. Retorna `true` se o pedido foi aceite.

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

Remove o token de dispositivo de NVS e reinicia o chip. Após reinicialização o dispositivo não está registado — o fluxo de claim automático recomeça. Esta função não retorna.

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

Saída para o gestor de integrações — para cablagem do lado do produto (Moonraker chamber target callbacks, estado da impressora Bambu, etc.).

Requer `#include <integrations/common/link_integrations_manager.h>`.

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

Saída para o cliente MQTT do SDK — para componentes que publicam os seus próprios tópicos ou se integram no encaminhamento de comandos (p.ex., `MenuBridge`).

Requer `#include <mqtt/mqtt_client.h>`.

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

Saída para o auxiliar de publicação dupla — envia uma carga para MQTT e Local WS simultaneamente. Utilizar para respostas de produto que têm de chegar ao cliente LAN da mesma forma que a telemetria publicada automaticamente.

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

Saída para o runtime do SDK — utilizado para definir um manipulador de comando completo em vez do despachante da fachada. Após `setCommandHandler(...)` a fachada `onRequest`/`onProfile` já não é invocada pela via MQTT.

**Importante:** invocar estritamente após `begin()` — `begin()` instala o seu próprio despachante, que tem de ser sobrescrito.

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// Assinatura do manipulador:
// void handleCommand(const char* cmd, JsonObjectConst data);
```

Requer `#include <runtime/idryer_runtime.h>`.

---

### Campos de telemetria {#telemetry-fields}

Preenchidos pelo produto em `loop()`. O SDK lê-os no temporizador `telemetryPeriodMs` e publica para MQTT e Local WS.

| Campo | Tipo | Sinalizador de configuração | Fim |
|-------|------|------------------------------|-----|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | Temperatura do ar, °C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | Humidade, % |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | Temperatura do aquecedor, °C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | Potência do aquecedor, 0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | Estado da ventoinha |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | Peso, gramas |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

`unitId` = 0 para a primeira (ou única) unidade. O índice deve ser < `Config.unitsCount`.

Campos `Status` — mesma estrutura, mas para estado operacional:

| Campo | Tipo | Fim |
|-------|------|-----|
| `status.mode[unitId]` | `UnitMode` | Modo atual da unidade |
| `status.targetTempC[unitId]` | `float` | Temperatura alvo |
| `status.durationS[unitId]` | `uint32_t` | Duração solicitada, s (0 = indefinida) |
| `status.elapsedS[unitId]` | `uint32_t` | Tempo decorrido desde início da sessão, s |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### Registo de retorno de chamada via runtime

Se for necessário controlo total sobre comandos recebidos (p.ex., o produto processa `get_config`, `set`, `invoke` não-padrão):

```cpp
// Assinatura — de idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// Registo — estritamente após link.begin()
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — cadeia de comando (`"set"`, `"invoke"`, `"ping"`, `"get_config"`).
`data` — ArduinoJson `JsonObjectConst` com carga.

Com esta abordagem, `onRequest()` e `onProfile()` não são invocadas da via MQTT — o produto processa comandos diretamente.

---

## Enumerações

### `iDryer::DeviceType`

| Valor | Numérico | Fim |
|-------|----------|-----|
| `Unknown` | 0 | Nenhum / indefinido |
| `Dryer` | 1 | Secador (iDryer LINK) |
| `Heater` | 2 | Aquecedor |
| `StorageLink` | 4 | Storage Link (ESP32-C3 + LED) |
| `IHeaterLink` | 5 | iHeater Link |

### `iDryer::UnitMode`

`Idle`, `Drying`, `Storage`, `Profile`, `Fault`, `Unknown`

### `iDryer::EventKind`

`Info`, `Warning`, `Error`

### `iDryer::RequestKind`

`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`

### `iDryer::IntegrationKind`

`Ha`, `Bambu`, `Moonraker`

### `iDryer::IntegrationState`

`Disabled`, `Idle`, `Connecting`, `Online`, `ConfigMissing`, `Error`

---

## Quando ir mais fundo

A fachada é suficiente para a maioria das tarefas. Se precisares de trabalhar abaixo do nível da fachada — com `idryer::IdryerRuntime`, `idryer::MqttClient`, `idryer::cloud::LinkIntegrationsManager` — consulta a secção Arquitetura.
