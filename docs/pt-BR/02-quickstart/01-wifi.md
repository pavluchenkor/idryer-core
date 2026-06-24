Depois deste passo seu ESP32 estará conectado ao WiFi e as credenciais serão salvas em NVS para reconexão automática no próximo reinício. Portal e MQTT vêm no próximo passo.


**Hardware:**

- Placa ESP32-C3 (DevKit, Super Mini, ou compatível)
- Cabo USB (USB-C ou Micro-USB dependendo de sua placa)

**Software:**

- PlatformIO em VS Code
- Navegador Chrome ou Edge (Web Serial API não é suportado em Safari ou Firefox)


**1. Crie `platformio.ini`** na raiz do seu projeto:

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Substitua `board` pelo valor para sua placa (`esp32-c3-devkitm-1`, `seeed_xiao_esp32c3`, etc.).

**2. Copie o exemplo.** Pegue o conteúdo de [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) e salve como `src/main.cpp` no seu projeto.

**3. Defina a ChipFamily.** No arquivo copiado, encontre a linha:

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

Certifique-se de que a ChipFamily corresponde ao seu chip: `CF_ESP32_C3`, `CF_ESP32_S3`, ou `CF_ESP32`.

**4. Flash:**

```bash
pio run -e improv-demo -t upload
```

**5. Abra [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)** no Chrome ou Edge. Clique em **Connect** e selecione a porta USB do dispositivo no diálogo do navegador.

**6. Insira o SSID e a senha** da sua rede 2,4 GHz. A página web enviará as credenciais para a placa via Serial-Improv. A placa as salvará em NVS.


Abra o Serial Monitor:

```bash
pio device monitor -b 115200
```

Após uma conexão bem-sucedida você verá:

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

Se esta linha não aparecer, consulte o link de solução de problemas abaixo.

!!! note
    Se as credenciais já estão salvas em NVS de uma execução anterior, a placa se conecta ao WiFi na inicialização automaticamente — Improv não é necessário.


- [02-claim.md](02-claim.md) — vincule o dispositivo à sua conta idryer.org.
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — se o WiFi não se conectar.

---
