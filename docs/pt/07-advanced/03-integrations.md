# Integrações de impressoras

O módulo de integrações permite an iDryer/iHeater device to connect to third-party systems: Home Assistant, Bambu Lab (LAN), Moonraker/Klipper. Incluir separadamente:

```cpp
#include <idryer_integrations.h>
```

**Integrações são um módulo opcional.** Storage Link não as utiliza. Elas são implementadas para iDryer LINK and iHeater LINK.

## LinkIntegrationsManager

Classe principal do módulo. Gere uma integração ativa de cada vez. Ligada através de o `CommandHandler` do produto — o mesmo handler utilizado para MQTT and local WS.

```cpp
LinkIntegrationsStore intStore;
idryer::cloud::LinkIntegrationsManager intManager(&s_mqtt, &intStore);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "link_integration") == 0) {
        intManager.handleLinkIntegrationCommand(data); return;
    }
    if (strcmp(cmd, "bambu_apply") == 0) {
        intManager.handleBambuApplyCommand(data); return;
    }
    // ... other product commands ...
}

// in setup():
runtime.setCommandHandler(handleCommand);
local.setCommandSink(handleCommand);
intManager.begin(); // after runtime.begin()
// in loop(): intManager.loop();
```

O gerenciador armazena configurações for all three integrations in NVS via `LinkIntegrationsStore`. Alternar a integração ativa is done with the command:

```json
{"active": "bambu"}     // or "ha", "moonraker", "none"
```

O estado é publicado em `idryer/{serial}/integrations/status` (retained) na alteração e a cada 30 segundos.

## Bambu Lab

`BambuClient` connects to the printer sobre MQTT em a rede local (TLS, porta 8883, certificado auto-assinado, `setInsecure`).

Dois modos de operação dependendo do tipo de dispositivo:

| Mode | DeviceType | Behaviour |
|------|-----------|-----------|
| **Escritor** | Dryer | envia `ams_filament_setting` para a impressora em `bambu_apply` |
| **Leitor** | Heater / IHeaterLink | subscreve a `device/{printerSerial}/report`, passa o status da impressora para um callback |

Parâmetros de conexão:

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

Reconectar com recuo exponencial de 1 s a 60 s.

Callbacks:

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — conexão ao broker MQTT de HA (não a nuvem de HA, mas o servidor MQTT HA integrado).

Configurado via o comando `link_integration`:

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

O adaptador suporta descoberta de host mDNS (string `homeassistant.local`) and conexão IP directa. Reconectar com recuo.

`HaMqttClient` é exposto via `intManager.haMqttClient()` — o produto pode publicar entidades HA através dele.

O dispositivo deve definir seu ID de cliente:

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

``MoonrakerClient` conecta via WebSocket (`ws://host:port/websocket`) and usa JSON-RPC 2.0 para subscrever objetos Klipper.

Caso de uso principal — iHeater: receber a temperatura alvo da câmara via `gcode_macro VIRTUAL_CHAMBER`.

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

O cliente subscreve a objetos Klipper, incluindo `gcode_macro VIRTUAL_CHAMBER`, `print_stats`, `display_status`, and sensores de temperatura.

Callbacks:

```cpp
intManager.setVirtualChamberCallback([](const VirtualChamberData& vc) {
    // vc.target — chamber target temperature
    // vc.available — VIRTUAL_CHAMBER object visible in Klipper
});

intManager.setMoonrakerStatusCallback([](const MoonrakerStatus& s) {
    // s.printerState, s.nozzleTemp, s.progress, ...
});
```

## Limitações

- Uma integração ativa de cada vez. A comutação é atômica: the a antiga para, the a nova começa.
- Uma instância de `BambuClient` por dispositivo (singleton via um apontador estático).
- ``LinkIntegrationsStore` armazena configuração em NVS — as definições persistem após reinicializações.
- O dispositivo deve especificar seu tipo (`setDeviceType`) para selecção correta do modo Bambu:
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // ou Heater, IHeaterLink
  ```
