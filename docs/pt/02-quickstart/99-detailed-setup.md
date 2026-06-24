Se é a primeira vez aqui — vá para [Comece em 5 minutos](01-five-minutes.md); esta página cobre configuração avançada e resolução de problemas.

Caminho rápido: ligue a biblioteca, grave um exemplo, veja o LED a piscar e o dispositivo no portal.


- Placa ESP32 (recomendado: ESP32-C3 DevKit, Super Mini, XIAO ESP32-S3, Waveshare ESP32-S3 Zero).
- PlatformIO com framework `arduino`, plataforma `espressif32`.
- WiFi 2,4 GHz com acesso à internet.
- Conta em [portal.idryer.org](https://portal.idryer.org/) para claim.


Em `platformio.ini` do seu produto:

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; apenas necessário para mqtt_with_local_ws

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```


Copie [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) para `include/secrets.h` no seu projecto e preencha seu SSID/palavra-passe. O ficheiro deve estar em `.gitignore`.

```cpp
```

`IDRYER_API_BASE` é normalmente definido via `build_flags`, não através de secrets.h.


O mais simples é [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino). Copie-o como seu ponto de partida:

- Não requer sensores, periféricos ou LAN WS.
- Não requer `handleCommand` manual — o fallback integrado em `IdryerRuntime` lida com comandos básicos.
- LED pisca quando o dispositivo está online — esse é o indicador de sucesso.


```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

Sequência de registo esperada:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← à espera de claim
[CSM] PIN: 1234567   expires in 600s          ← se auto-claim está activado
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← pronto, LED começa a piscar
[RT]  Cloud Online
```


Auto-claim já está activado no exemplo. O PIN aparece no registo. Introduza-o em [portal.idryer.org](https://portal.idryer.org/) → "Add device". Após claim, `CloudStateMachine` faz a transição para `Online`.


Os exemplos seguintes introduzem cada um um novo nível de complexidade:

| Exemplo | O que é adicionado |
|---------|-------------------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | `handleCommand` personalizado, manipulação de `commands/invoke` e `commands/set` |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | Provisionamento WiFi via Improv (sem credenciais hardcoded) |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | Servidor WebSocket LAN local + `DevicePublisher` (uma publicação — dois transportes) |


Um caminho alternativo para desenvolvedores — ver o fluxo de claim completo directamente num monitor Serial padrão, sem Improv e sem UI do portal.

Em `platformio.ini`, crie um env dev com a bandeira `-DIDRYER_DEV_REPL=1`:

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

O que a bandeira activa:
- Os registos HAL em `Serial` começam **imediatamente** do arranque (sem silêncio até o WiFi ligar).
- O provisionamento Improv é **desactivado** — Serial está livre para comandos interactivos.
- Um REPL simples aparece em `main.cpp`: `wifi`, `claim`, `status`, `wipe`, `restart`, `help`.

Fluxo completo:

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

No monitor:

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — enter in portal
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

O REPL aceita comandos independentemente da definição de fim de linha no monitor Serial (`\n`, `\r`, ou timeout inactivo 120 ms) — funciona em qualquer terminal, incluindo `pio device monitor`, Arduino IDE Serial Monitor, `screen`, `picocom`.

A compilação de produção (`-e my-device-prod`, sem `IDRYER_DEV_REPL`) usa Improv via Chrome (`https://www.improv-wifi.com/`) e não contém código REPL — a bandeira é compile-time, poupando Flash.

`secrets.h` com `WIFI_SSID/WIFI_PASSWORD` (Passo 2) permanece um caminho separado para cenários CI/auto-flash headless — funciona em ambos os ambientes.

Depois de qualquer um dos exemplos estar funcionando, leia:

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — ordem de objectos em `main.cpp`.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — como os dados se movem.
- [04-patterns/](../04-patterns/) — receitas: adicione sensor, periférico, transporte.
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — lista completa para um novo produto.
- [10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — o que fazer se a stack ficar travada.
