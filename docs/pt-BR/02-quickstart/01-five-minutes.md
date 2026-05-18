# Comece em 5 minutos

Depois desta página seu ESP32 estará programado, conectado ao WiFi e aparecerá em [portal.idryer.org](https://portal.idryer.org/) com status Online. Requisitos: ESP32-C3 (DevKit, Super Mini ou compatível), cabo USB, PlatformIO no VS Code.

## 1. Prepare secrets.h

Copie [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) para `include/secrets.h` em seu projeto e defina seu SSID WiFi e senha (apenas 2.4 GHz):

```cpp
#define WIFI_SSID      "seu-ssid"
#define WIFI_PASSWORD  "sua-senha"
```

Adicione `include/secrets.h` ao `.gitignore`.

## 2. Configure platformio.ini

Crie `platformio.ini` na raiz do projeto:

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://caminho/para/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

Altere `board` para corresponder à sua placa. Substitua `caminho/para/idryer-core` pelo caminho real para a biblioteca.

## 3. Copie o exemplo 01_blink_status

Copie o conteúdo de [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) em `src/main.cpp` do seu projeto. O exemplo não requer sensores ou dependências adicionais — apenas uma raiz de composição mínima.

## 4. Programa

```bash
pio run -e blink-demo -t upload
```

## 5. Abra o Monitor Serial

```bash
pio device monitor -b 115200
```

Sequência de log esperada:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

Depois de digitar o PIN no portal (passo 6):

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Se o dispositivo parou na mensagem `PIN: ...` — isso é esperado; prossiga para a etapa 6.

## 6. Reivindicar o dispositivo no portal

Abra [portal.idryer.org](https://portal.idryer.org/), vá para **Add device** e digite o PIN do Serial Monitor. Após uma reivindicação bem-sucedida, o dispositivo fará a transição para `Online` e o LED embutido piscará a cada 500 ms.

Fluxo de reivindicação detalhado: [Onboarding](02-onboarding.md).

## O que fazer a seguir

- Adicione um sensor — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- Adicione um periférico — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- Referência completa da API — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Como funciona internamente — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
