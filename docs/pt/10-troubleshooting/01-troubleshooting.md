# Resolução de Problemas

Sintomas comuns ao trabalhar com `idryer-core`, suas causas e soluções.

Antes de ler, certifique-se de que os registos HAL estão ativados (`idryer::hal::initArduinoHal(&Serial)`) e que `-DCORE_DEBUG_LEVEL=3` ou superior está definido em `platformio.ini`.

## WiFi

### Máquina de estado presa em `WifiConnecting`

Sintomas: o registo repete `state: WifiConnecting`, a transição para `Provisioning` nunca acontece.

Possíveis causas:

- SSID/palavra-passe incorreta. Verifique `WIFI_SSID` / `WIFI_PASSWORD` em `secrets.h`. Após provisioning Improv, as credenciais vêm do NVS, não de `secrets.h`.
- Rede 5 GHz. ESP32 suporta apenas 2,4 GHz.
- Rede oculta ou filtro MAC no router.
- `WiFi.begin()` chamado antes de `idryer::hal::initArduinoHal(...)` — sem saída de registo, mas isto não é a causa da suspensão, apenas cegueira.

O que verificar:

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFi conecta mas cai após 30–60 segundos

Tipicamente: sinal fraco (`RSSI < -80 dBm`), ESP32-C3 alimentado por um hub USB sem alimentação dedicada 5V/1A, conflito com tarefas FreeRTOS.

Registar RSSI no loop do produto:

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## Provisioning e reclamação

### Máquina de estado presa em `Provisioning`

Sintomas: `state: Provisioning` sem transição para `Registering` ou `AwaitingClaim`.

Causas:

- `IDRYER_API_BASE` incorreta nas build_flags. Deve ser `https://portal.idryer.org/api` (produção) ou `https://staging.idryer.org/api` (encenação).
- Certificado TLS ausente (Let's Encrypt ISRG Root X1). Incorporado em `root_ca.h`, mas quando compilado sem `MQTT_USE_TLS`, o cliente HTTP também usa TLS — a AC raiz é necessária para a API HTTP também.
- Hora do dispositivo não sincronizada (o handshake TLS requer uma data válida). Verifique se `configTime(...)` é chamado em `setStateChangeCallback` após sair de `WifiConnecting` (como em Storage Link).

### Máquina de estado presa em `AwaitingClaim`

Este é o estado normal enquanto o utilizador não tiver entrado no PIN no portal. O PIN é impresso no registo via `setClaimPinCallback`.

Para reclamação automática (dispositivos autónomos sem UI):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Após `requestClaim()`, o backend emite um PIN que o utilizador deve entrar no portal.

### `seedSerialFromMac()` gerou um serial, mas um diferente foi entrado no portal

O serial armazenado em NVS tem prioridade sobre a geração de MAC. `seedSerialFromMac()` escreve para NVS apenas se nenhum serial estiver presente ainda. Para mudar o serial, limpe o NVS:

```cpp
s_credentials.clear();
```

## MQTT

### Máquina de estado entrou `MqttConnecting` mas não atinge `Online`

Causas:

- Broker inacessível. Produção: `mqtt.idryer.org:8883`, encenação: `staging.idryer.org:1884`.
- `MQTT_USE_TLS=1` sem uma AC raiz correta — o handshake falha silenciosamente.
- `setBufferSize(16384)` não aplicado — o buffer `PubSubClient` é 256 bytes por padrão. `MqttClient` já define 16384, mas se usar `PubSubClient` diretamente — defina o buffer você mesmo.
- Sessão persistente "presa" no broker com um ID de cliente diferente. Limpe NVS e re-flash.

### Comandos do backend não chegam

Verifique a subscrição — `MqttClient` subscreve a `idryer/{serial}/commands/#` com QoS 1. Se a subscrição falhar, o registo mostrará:

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

Verifique se `setCommandHandler()` é chamado **antes** de `runtime.begin()` — caso contrário, o primeiro lote de comandos pode ser perdido.

### `PubSubClient` desconecta em intervalos exatos de 60 segundos

Este é um timeout de keep-alive. Seu loop MQTT pode não ser chamado com frequência suficiente — `s_runtime.loop()` deve girar sem blocos longos. Verifique se `loop()` não tem `delay(>500ms)` e nenhuma chamada de rede bloqueante.

## Comandos e manipuladores

### `commands/invoke` chega mas `ActionDispatcher` não é chamado

Se registou `setCommandHandler()`, **o fallback incorporado para `ActionDispatcher` está desativado**. `IdryerRuntime` passa tudo (exceto `ping`) para seu `CommandHandler`. Você deve chamar explicitamente `s_dispatcher.handleInvoke(data)` lá para comandos `invoke`.

Modelo:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... product commands ...
}
```

### `commands/set` recebido mas configuração não foi aplicada

`ActionDispatcher::handleSet` extrai `id` e `val` e os passa para o `SetCallback` registado. Verifique se:

- `dispatcher.setSetCallback(onSetCommand, nullptr)` é chamado em `setup()`.
- `onSetCommand` realmente chama `s_profile.applyConfig(id, val)`.
- `applyConfig` retorna `true` para valores `id` conhecidos. Para desconhecidos retorna `false` e as alterações são ignoradas.

## Telemetria

### Telemetria não é publicada

`idryer-core` não publica telemetria automaticamente. O código do produto sempre faz isto.

Verifique se:

- `pub.publishTelemetry(doc)` (ou `s_mqtt.publishTelemetry(doc)` se LocalAccess não for usado) é realmente chamado em `loop()`.
- A condição de taxa não está bloqueando todas as chamadas. Um erro comum:
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  Na primeira passagem `lastTm == 0` e `millis()` ainda é pequeno — o ramo nunca executa. Use `>=` e inicialize `lastTm` na primeira passagem.
- `s_runtime.isOnline() == true`. MQTT está desconectado antes de Online — a publicação não passará.
- O tamanho de `JsonDocument` é suficiente para o payload. Verifique `doc.overflowed()` após `serializeJson`.

### `publishTelemetry` retorna `false`

Causas:

- Não conectado ao broker (`MqttClient::isConnected() == false`).
- Buffer excedido — payload maior que `MQTT_BUFFER_SIZE` (16384 bytes). Para dados grandes use `publishConfigRaw` (com chunks) ou reduza o payload.

### `DevicePublisher::publishTelemetry` não chega ao cliente WS

`DevicePublisher` não retorna um erro se o cliente WS não estiver conectado — simplesmente pula a parte WS. Verifique `s_local.isClientConnected()`. Se `false` — o cliente não está autenticado ou não está conectado.

## NTP e hora do sistema

### Hora do dispositivo não sincronizada

A sincronização NTP é iniciada em `setStateChangeCallback` após a primeira saída de `WifiConnecting`:

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

Se este callback não for registado — o tempo não é sincronizado automaticamente. Um handshake TLS para o broker requer hora válida; caso contrário, o certificado é considerado expirado ou do futuro.

Canal alternativo: `IdryerRuntime` manipula `commands/ping` e aplica `data["timestamp"]` via `settimeofday()`. Se o backend enviar ping uma vez por minuto — o tempo é atualizado sem NTP.

### O handshake TLS falha após longo tempo de atividade

Se o servidor NTP for inacessível e o dispositivo funcionar sem reinicialização durante muito tempo, o tempo pode desviar (especialmente em ESP32-C3 sem TCXO). Sintoma: `connection failed` repentino após vários dias de atividade.

Solução: assegure-se de que `pool.ntp.org` é acessível da sua rede, ou receba `commands/ping` do backend mais frequentemente.

### `getIsoTimestamp` retorna ano 1970

O tempo do sistema ainda não foi sincronizado. O tempo aparece após o primeiro `configTime` bem-sucedido ou `commands/ping`. Até então, `info`/`telemetry` será publicado com um espaço reservado.

## ArduinoJson

### Erro de compilação: `StaticJsonDocument` não é membro de `ArduinoJson`

Você está usando ArduinoJson v7. O tipo `StaticJsonDocument` existe apenas em v6. Soluções:

- Pindel v6 em `platformio.ini`:
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- Ou migre seu código para a API v7 (`JsonDocument` em vez de `StaticJsonDocument<N>`). `idryer-core` é escrito para v6.

### Erro de compilação: sobrecarga ambígua ou incompatibilidade de tipo

Duas versões de ArduinoJson podem acabar num projeto através de dependências transitivas. Verifique:

```bash
pio pkg list -e my-device | grep -i arduinojson
```

Deve haver **uma** versão. Se houver duas — piá-a explicitamente via `lib_deps` e se necessário via `lib_ldf_mode = chain+` ou `lib_ignore`.

### `doc.overflowed()` verdadeiro após serializeJson

O tamanho de `StaticJsonDocument<N>` é muito pequeno para o payload. Aumente `N` ou use `DynamicJsonDocument` para caminhos infrequentemente chamados.

## WS Local (LocalAccess)

### App não descobre o dispositivo em LAN

mDNS deve ser iniciado **imediatamente após o número de série estar disponível** via `s_local.initMdns(serial)`. Verifique se:

- O router não bloqueia multicast.
- A aplicação está procurando `_idryer._tcp` na porta 81.
- O número de série do dispositivo corresponde ao registado no portal.

### Cliente WS conectado mas recebe `auth_required`

A primeira mensagem do cliente deve ser `{"type":"auth","token":"<device_token>"}`. Se o token for inválido, `LocalAccess` chama `setTokenRefreshCallback()`. O produto deve nesse callback re-ler o token de `ICredentialStore` e chamar `s_local.updateToken(...)`.

## Memória e estabilidade

### Memória livre diminui ao longo do tempo

`PubSubClient::loop()` e `WebSocketsServer::loop()` não devem vazar, mas verifique seu código do produto:

- Crie `JsonDocument` na pilha (`StaticJsonDocument<N>`), não no heap (`DynamicJsonDocument`) para caminhos frequentemente chamados.
- `String` em código de produto em ESP32-C3 rapidamente fragmenta o heap — use `char[]` e `snprintf`.

### `Stack overflow` ou `Guru Meditation`

`s_runtime.loop()` não gera tarefas FreeRTOS — tudo funciona no loop Arduino. Se houver um crashe de pilha, procure por:

- Grande `JsonDocument` local/`char[8192]` na pilha do loop Arduino (padrão 8 KB).
- Recursão profunda em código do produto.

Aumente a pilha do loop Arduino:

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi (provisioning via Serial)

### Improv não aceita credenciais

Improv deve ser proprietário de `Serial` até que as credenciais sejam recebidas:

```cpp
idryer::hal::initArduinoHal(nullptr);   // logs to /dev/null while Improv holds Serial
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // restore log output
}
```

Se `HAL_LOG_*` escreve para `Serial` em paralelo com o protocolo Improv, Improv falha na checksum.

### Cliente Improv não vê o dispositivo

Verifique `ChipFamily` em `setDeviceInfo`. Deve corresponder ao chip real: `CF_ESP32_C3`, `CF_ESP32_S3`, `CF_ESP32_S2`, `CF_ESP32`. Uma incompatibilidade — o cliente Improv não mostrará o dispositivo na lista.

Também assegure-se de que a taxa de baud Serial é 115200. O protocolo Improv espera isto.

## Diagnóstico de integração

### Saída de diagnóstico completo (1 Hz)

Menu → `DIAGNOSTICS → DIAG LOG` (`menu.diag_en`). Desativado por padrão.
Ativar via UI do dispositivo, portal (`commands/set` com `bind=diag_en`),
ou REPL (`set diag_en 1`).

Quando ativado, um bloco é impresso para Serial uma vez por segundo:

```
=========== iHeater Link diagnostics ===========
[device]    serial=DEVICE_... online=1 uptime=42s
[wifi]      status=3 ssid=Apart_4 ip=192.168.0.140 rssi=-51
[rmt-out]   mode=DRYING target=70.0°C
[active]    bambu
[bambu]     state=CONNECTED  ip=192.168.0.171 serial=<set> lan=<set>
            gcode_state='RUNNING' tray='PLA' chamber_target=0.0 chamber_temp=0.0
[moonraker] state=DISABLED   ws=ws://192.168.0.171:7125
            vc.available=0 vc.target=0.0 vc.temp=0.0 vc.has_sensor=0
[ha]        state=DISABLED   host=<empty>:1883 user=<empty>
[menu]      bambu_en=1 moon_en=0 ha_en=0 diag_en=1  mat_pla=45 ...
================================================
```

Útil para diagnóstico remoto: o utilizador ativa `DIAG LOG`, copia a saída → estados de conector, lastError, e o que está realmente indo para RMT são visíveis.

### Canal ANOMALY (baseado em eventos)

Independentemente de `diag_en`, conectores e helpers escrevem linhas separadas com
o prefixo `[!] ANOMALY` em condições inesperadas:

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

O prefixo `[!]` destaca visualmente a anomalia no fluxo de registo geral. Esta é a primeira coisa a procurar em Serial quando algo "não está funcionando".

### Auto-OFF na perda de conexão (fail-safe)

Se a integração ativa perde sua conexão (TCP/WS disconnect), o conector
imediatamente redefine a temperatura alvo:

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`, `available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF.
- **Bambu** — transição `Connected → !Connected` → `chamberTarget=0`, `trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF.
- **HA** — fail-safe ainda não implementado.

Sem esta lógica, o aquecimento continuaria na última temperatura alvo conhecida até
a conexão ser restaurada.

### Bambu: filtro gcode_state

`auto_heat` aquece **apenas** quando `gcode_state == "RUNNING"` ou `"PREPARE"`.
Todos os outros estados (`IDLE`, `FINISH`, `FAILED`, `PAUSE`, `INIT`, `OFFLINE`,
`SLICING`, `UNKNOWN`, vazio) → OFF.

Ao diagnosticar, preste atenção ao `gcode_state` na linha de diagnósticos `[bambu]` — se mostrar `IDLE`/`FINISH`, não haverá aquecimento independentemente de se uma bandeja ativa está presente.

### Bancos de testes para depuração sem uma impressora

Para testar integrações sem impressoras reais, repositórios de produtos
(por exemplo, iHeater-link) podem conter utilitários stub
como `fake_moonraker` / `fake_bambu` que enviam uma rampa de valores
a cada 30 segundos.
