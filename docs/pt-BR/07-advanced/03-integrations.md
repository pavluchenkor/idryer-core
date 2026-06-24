# Integrações de impressoras

O módulo de integrações permite que um dispositivo iDryer/iHeater se conecte a sistemas de terceiros: Home Assistant, Bambu Lab (LAN), Moonraker/Klipper. Inclua separadamente:

```cpp
#include <idryer_integrations.h>
```

**As integrações são um módulo opcional.** Storage Link não as usa. Elas são implementadas para iDryer LINK e iHeater LINK.

## LinkIntegrationsManager

Classe principal do módulo. Gerencia uma integração ativa por vez. Conectada através do `CommandHandler` do produto — o mesmo manipulador usado para MQTT e WS local.

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

O gerenciador armazena configurações para todas as três integrações em NVS via `LinkIntegrationsStore`. Mudar a integração ativa é feito com o comando:

```json
{"active": "bambu"}     // or "ha", "moonraker", "none"
```

O estado é publicado em `idryer/{serial}/integrations/status` (retido) na mudança e a cada 30 segundos.

## Bambu Lab

`BambuClient` conecta à impressora sobre MQTT na rede local (TLS, porta 8883, certificado auto-assinado, `setInsecure`).

Dois modos operacionais dependendo do tipo de dispositivo:

| Modo | DeviceType | Comportamento |
|------|-----------|-----------|
| **Writer** | Dryer | envia `ams_filament_setting` para a impressora em `bambu_apply` |
| **Reader** | Heater / IHeaterLink | subscreve a `device/{printerSerial}/report`, passa status da impressora para um callback |

Parâmetros de conexão:

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

Reconectar com backoff exponencial de 1 s a 60 s.

Callbacks:

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — conexão ao broker MQTT do HA (não a nuvem do HA, mas o servidor MQTT integrado do HA).

Configurado via comando `link_integration`:

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

O adaptador suporta descoberta de host mDNS (string `homeassistant.local`) e conexão IP direta. Reconectar com backoff.

`HaMqttClient` é exposto via `intManager.haMqttClient()` — o produto pode publicar entidades HA através dele.

O dispositivo deve definir seu ID de cliente:

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` conecta via WebSocket (`ws://host:port/websocket`) e usa JSON-RPC 2.0 para subscrever objetos Klipper.

Caso de uso principal — iHeater: receber a temperatura alvo da câmara via `gcode_macro VIRTUAL_CHAMBER`.

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

O cliente subscreve a objetos Klipper incluindo `gcode_macro VIRTUAL_CHAMBER`, `print_stats`, `display_status` e sensores de temperatura.

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

- Uma integração ativa por vez. Mudar é atômico: a antiga para, a nova começa.
- Uma instância `BambuClient` por dispositivo (singleton via ponteiro estático).
- `LinkIntegrationsStore` armazena configuração em NVS — as configurações persistem entre reinicializações.
- O dispositivo deve especificar seu tipo (`setDeviceType`) para seleção de modo Bambu correto:
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // or Heater, IHeaterLink
  ```
