# 打印機集成

The integrations module allows an iDryer/iHeater device to connect to third-party systems: Home Assistant, Bambu Lab (LAN), Moonraker/Klipper. Include separately:

```cpp
#include <idryer_integrations.h>
```

**Integrations are an optional module.** Storage Link does not use them. They are implemented for iDryer LINK and iHeater LINK.

## LinkIntegrationsManager

Main class of the module. Manages one active integration at a time. Wired in through the product's `CommandHandler` — the same handler used for MQTT and local WS.

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

The manager stores configurations for all three integrations in NVS via `LinkIntegrationsStore`. Switching the active integration is done with the command:

```json
{"active": "bambu"}     // or "ha", "moonraker", "none"
```

State is published to `idryer/{serial}/integrations/status` (retained) on change and every 30 seconds.

## Bambu Lab

`BambuClient` connects to the printer over MQTT on the local network (TLS, port 8883, self-signed cert, `setInsecure`).

Two operating modes depending on device type:

| Mode | DeviceType | Behaviour |
|------|-----------|-----------|
| **Writer** | Dryer | sends `ams_filament_setting` to the printer on `bambu_apply` |
| **Reader** | Heater / IHeaterLink | subscribes to `device/{printerSerial}/report`, passes printer status to a callback |

Connection parameters:

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

Reconnect with exponential backoff from 1 s to 60 s.

Callbacks:

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — connection to the HA MQTT broker (not the HA cloud, but the built-in HA MQTT server).

Configured via the `link_integration` command:

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

The adapter supports mDNS host discovery (string `homeassistant.local`) and direct IP connection. Reconnect with backoff.

`HaMqttClient` is exposed via `intManager.haMqttClient()` — the product can publish HA entities through it.

The device must set its client ID:

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` connects via WebSocket (`ws://host:port/websocket`) and uses JSON-RPC 2.0 to subscribe to Klipper objects.

Primary use case — iHeater: receiving the chamber target temperature via `gcode_macro VIRTUAL_CHAMBER`.

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

The client subscribes to Klipper objects including `gcode_macro VIRTUAL_CHAMBER`, `print_stats`, `display_status`, and temperature sensors.

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

## 限制

- One active integration at a time. Switching is atomic: the old one stops, the new one starts.
- One `BambuClient` instance per device (singleton via a static pointer).
- `LinkIntegrationsStore` stores configuration in NVS — settings persist across reboots.
- The device must specify its type (`setDeviceType`) for correct Bambu mode selection:
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // or Heater, IHeaterLink
  ```
