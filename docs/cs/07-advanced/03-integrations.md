# Integrace tiskárny

Modul integrace umožňuje zařízení iDryer/iHeater připojit se k systémům třetích stran: Home Assistant, Bambu Lab (LAN), Moonraker/Klipper. Zahrňte samostatně:

```cpp
#include <idryer_integrations.h>
```

**Integrace jsou volitelný modul.** Storage Link je nepoužívá. Jsou implementovány pro iDryer LINK a iHeater LINK.

## LinkIntegrationsManager

Hlavní třída modulu. Spravuje jednu aktivní integraci najednou. Zapojeno prostřednictvím objektu produktu `CommandHandler` — stejný handler použitý pro MQTT a místní WS.

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
    // ... ostatní příkazy produktu ...
}

// v setup():
runtime.setCommandHandler(handleCommand);
local.setCommandSink(handleCommand);
intManager.begin(); // po runtime.begin()
// v loop(): intManager.loop();
```

Manager ukládá konfigurace všech tří integrací v NVS prostřednictvím `LinkIntegrationsStore`. Přepínání aktivní integrace se provádí příkazem:

```json
{"active": "bambu"}     // nebo "ha", "moonraker", "none"
```

Stav je publikován na `idryer/{serial}/integrations/status` (zachován) při změně a každých 30 sekund.

## Bambu Lab

`BambuClient` se připojuje k tiskárně přes MQTT v místní síti (TLS, port 8883, vlastnoruční podepsaný certifikát, `setInsecure`).

Dva režimy provozu v závislosti na typu zařízení:

| Režim | DeviceType | Chování |
|------|-----------|-----------|
| **Writer** | Dryer | odešle `ams_filament_setting` na tiskárnu při `bambu_apply` |
| **Reader** | Heater / IHeaterLink | přihlásí se k odběru `device/{printerSerial}/report`, předá stav tiskárny zpětnému volání |

Parametry připojení:

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

Připojení s exponenciálním zpátky od 1 s do 60 s.

Zpětná volání:

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — připojení k brokerovi HA MQTT (nikoli ke cloudu HA, ale k vestavěnému serveru HA MQTT).

Nakonfigurováno prostřednictvím příkazu `link_integration`:

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

Adaptér podporuje zjišťování hostů přes mDNS (řetězec `homeassistant.local`) a přímé připojení IP. Připojení s exponenciálním zpátky.

`HaMqttClient` je vystaveno prostřednictvím `intManager.haMqttClient()` — produkt může publikovat entity HA.

Zařízení musí nastavit své ID klienta:

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` se připojuje přes WebSocket (`ws://host:port/websocket`) a používá JSON-RPC 2.0 pro přihlášení k odběru objektů Klipperu.

Primární případ použití — iHeater: příjem cílové teploty komory prostřednictvím `gcode_macro VIRTUAL_CHAMBER`.

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

Klient se přihlásí k odběru objektů Klipperu včetně `gcode_macro VIRTUAL_CHAMBER`, `print_stats`, `display_status` a teplotních senzorů.

Zpětná volání:

```cpp
intManager.setVirtualChamberCallback([](const VirtualChamberData& vc) {
    // vc.target — cílová teplota komory
    // vc.available — objekt VIRTUAL_CHAMBER viditelný v Klipperu
});

intManager.setMoonrakerStatusCallback([](const MoonrakerStatus& s) {
    // s.printerState, s.nozzleTemp, s.progress, ...
});
```

## Omezení

- Jedna aktivní integrace najednou. Přepínání je atomické: stará se zastaví, nová se spustí.
- Jedna instance `BambuClient` na zařízení (singleton prostřednictvím statického ukazatele).
- `LinkIntegrationsStore` ukládá konfiguraci v NVS — nastavení přetrvává přes restartování.
- Zařízení musí zadat svůj typ (`setDeviceType`) pro správný výběr režimu Bambu:
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // nebo Heater, IHeaterLink
  ```
