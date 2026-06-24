# MqttClient

`MqttClient` é o cliente MQTT do dispositivo. Envolve `PubSubClient`, gerencia a conexão e encaminha mensagens recebidas. Todos os tópicos são formados automaticamente a partir do número de série do dispositivo.

## Inicialização

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

Chamado por `CloudStateMachine` após provisionamento bem-sucedido. Não se conecta imediatamente — define parâmetros e configura TLS.

Parâmetros:

- `serialNumber` — número de série do dispositivo. Utilizado como ID de cliente MQTT e nome de usuário.
- `token` — token do dispositivo. Utilizado como senha MQTT.

Quando compilado com a flag `MQTT_USE_TLS=1`, o cliente configura `WiFiClientSecure` com a CA raiz Let's Encrypt (incorporada em `root_ca.h`).

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // ver "Tamanho do buffer" abaixo
mqttClient_.setKeepAlive(60);
```

## Tamanho do buffer {#buffer-size}

`PubSubClient` utiliza um buffer de 256 bytes por padrão — suficiente apenas para mensagens curtas. Para dispositivos iDryer isto é muito pequeno: a carga útil principal "pesada" é a configuração do dispositivo (menu), que é publicada no tópico `idryer/{serial}/config` de uma só vez.

`MqttClient` define o buffer para `MQTT_BUFFER_SIZE` e limita o tamanho do fragmento para configurações grandes a `MQTT_CONFIG_CHUNK_SIZE`. Ambas as constantes estão definidas em `lib/idryer-core/src/mqtt/mqtt_client.h`:

```cpp
#define MQTT_BUFFER_SIZE        16384  // buffer PubSubClient
#define MQTT_CONFIG_CHUNK_SIZE  16000  // dados máximos num fragmento de configuração
```

Relação entre elas:

- `MQTT_BUFFER_SIZE` (16384 bytes) — limite superior para **uma mensagem MQTT**. Qualquer chamada de `publish*()` com carga útil maior que isto será abandonada por `PubSubClient` sem enviar.
- `MQTT_CONFIG_CHUNK_SIZE` (16000 bytes) — tamanho máximo de `"d"` (a porção de dados) dentro de um fragmento de `publishConfigRaw`. A margem de 384 bytes é reservada para o envelope do fragmento: `{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}` mais o campo `timestamp` adicionado automaticamente.

### Por que 16384

O número foi escolhido não por razões estéticas, mas pela **carga útil máxima esperada do dispositivo**, que é a transferência de configurações/menu:

- Armazenamento Link e configuração Link/iHeater (menu) serializa como JSON com escape. Um snapshot completo do menu atual cabe em ~10–14 KB.
- A margem até 16384 cobre crescimento do menu sem necessidade de dividir em fragmentos.
- O valor é múltiplo de 4 KB — conveniente para alocação em ESP32.

Se seu produto tiver uma configuração maior (por exemplo, um menu estendido com muitos itens ou valores binários), dois caminhos estão disponíveis:

1. **Aumentar `MQTT_BUFFER_SIZE`** — sobrescrever via `build_flags` em `platformio.ini`:
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   Tenha em mente o uso de RAM: `PubSubClient` mantém este buffer continuamente. Em ESP32-C3 (~400 KB de heap livre) 32 KB é aceitável, mas avançar mais começa a apresentar riscos.

2. **Utilizar `publishConfigRaw(json, length)`** — divide a carga útil em fragmentos de `MQTT_CONFIG_CHUNK_SIZE`; o backend os remonta pelos campos `tid` / `idx` / `total` / `last`. Este caminho é preferido para configurações provenientes de RP2040 sobre UART em peças de comprimento arbitrário.

### Aplica-se a publicações de produto

O mesmo limite de 16384 bytes aplica-se a `publishTelemetry`, `publishStatus`, `publishEvent`. Na prática, telemetria e eventos são muito menores (centenas de bytes); apenas publicações de configuração aproximam-se deste limite. Se seu projeto publicar periodicamente uma carga útil grande (por exemplo, um dump de matriz de medição), estime seu tamanho com antecedência ou divida-o você mesmo.

## Conexão

```cpp
bool MqttClient::connect();
```

Realiza:

1. Conexão ao broker com sessão persistente (`clean_session = false`). Sessão persistente é obrigatória — sem ela, comandos chegados enquanto o dispositivo está offline são perdidos.
2. Define a mensagem LWT no tópico `idryer/{serial}/offline` (QoS 1, não retido).
3. Se inscreve em `idryer/{serial}/commands/#` (QoS 1). Faz até 3 tentativas; em caso de falha, desconecta.

Retorna `true` se a conexão e inscrição foram bem-sucedidas.

## Loop

```cpp
void MqttClient::loop();
```

Chamado em cada iteração. Reconecta em desconexão, depois chama `PubSubClient::loop()` para receber mensagens recebidas.

## Publicação

Todos os métodos de publicação adicionam um campo `timestamp` (ISO 8601 UTC) se ainda não estiver presente no documento.

| Método | Tópico | Retido |
|--------|--------|--------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | sim |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | não |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | sim |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | não |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | não |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | sim |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | não |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | não |

`publishConfigRaw` divide automaticamente a carga útil em fragmentos se o tamanho exceder `MQTT_CONFIG_CHUNK_SIZE` (16000 bytes). Cada fragmento contém os campos `tid`, `idx`, `total`, `last`, `d`.

!!! note
    `PubSubClient` publica sempre em QoS 0, independentemente das configurações de tópico. Esta é uma limitação da biblioteca.

## Recebimento de comandos

Mensagens recebidas no tópico `idryer/{serial}/commands/{cmd}` são analisadas como JSON e passadas para o `CommandCallback` registrado:

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

A parte `{cmd}` é extraída do tópico e passada como primeiro argumento. `IdryerRuntime` registra este callback em `begin()`.

## Métodos auxiliares

```cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bytes
static char* generateUuid(char* buffer);    // buffer >= 37 bytes
```

`generateUuid` gera um UUID v4 baseado em `esp_random()`.

## Limitações

- Uma instância de `MqttClient` por dispositivo (singleton via `instance_`).
- Tamanho máximo de uma única mensagem JSON — `MQTT_BUFFER_SIZE` (padrão 16384 bytes). Dimensionado para a carga útil mais pesada do dispositivo — tipicamente a configuração serializada (menu). Para configurações maiores aumente a constante via `build_flags` ou utilize `publishConfigRaw` com divisão automática de fragmentos. Ver [Tamanho do buffer](#buffer-size).
- TLS está ativado pela flag de compilação `MQTT_USE_TLS`.
