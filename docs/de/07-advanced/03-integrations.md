# Drucker-Integrationen

Das Integrationsmodul erlaubt einem iDryer/iHeater-Gerät, sich mit Drittanbieter-Systemen zu verbinden: Home Assistant, Bambu Lab (LAN), Moonraker/Klipper. Separat einfügen:

```cpp
#include <idryer_integrations.h>
```

**Integrationen sind ein optionales Modul.** Storage Link verwendet sie nicht. Sie sind für iDryer LINK und iHeater LINK implementiert.

## LinkIntegrationsManager

Hauptklasse des Moduls. Verwaltet eine aktive Integration zur Zeit. Verdrahtet durch den Produkt-`CommandHandler` — der gleiche Handler, der für MQTT und lokales WS verwendet wird.

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

Der Manager speichert Konfigurationen für alle drei Integrationen in NVS über `LinkIntegrationsStore`. Das Umschalten der aktiven Integration erfolgt mit dem Befehl:

```json
{"active": "bambu"}     // or "ha", "moonraker", "none"
```

Der Status wird auf `idryer/{serial}/integrations/status` (beibehalten) bei Änderung und alle 30 Sekunden veröffentlicht.

## Bambu Lab

`BambuClient` verbindet sich mit dem Drucker über MQTT auf dem lokalen Netzwerk (TLS, Port 8883, selbstsigniertes Zert, `setInsecure`).

Zwei Betriebsmodi je nach Gerätetyp:

| Modus | DeviceType | Verhalten |
|------|-----------|-----------|
| **Schriftsteller** | Dryer | sendet `ams_filament_setting` zum Drucker auf `bambu_apply` |
| **Leser** | Heater / IHeaterLink | abonniert `device/{printerSerial}/report`, übergibt Druckerstatus an einen Callback |

Verbindungsparameter:

```cpp
BambuConfig cfg;
cfg.ip = "192.168.1.50";
cfg.serial = "PRINTER_SERIAL";
cfg.lanAccessCode = "LAN_CODE";
cfg.enabled = true;
bambuClient.configure(cfg);
```

Neuverbindung mit exponentiellem Backoff von 1 s bis 60 s.

Callbacks:

```cpp
bambuClient.setPrinterStatusCallback([](const BambuPrinterStatus& s) {
    // s.gcodeState, s.nozzleTemp, s.trayType, ...
});
```

## Home Assistant

`HaIntegrationAdapter` + `HaMqttClient` — Verbindung zum HA-MQTT-Broker (nicht zur HA-Cloud, aber zum eingebauten HA-MQTT-Server).

Konfiguriert über den `link_integration` Befehl:

```json
{"type": "ha", "enabled": true, "host": "homeassistant.local", "port": 1883, "username": "...", "password": "..."}
```

Der Adapter unterstützt mDNS-Hostnamen-Entdeckung (String `homeassistant.local`) und direkte IP-Verbindung. Neuverbindung mit Backoff.

`HaMqttClient` wird über `intManager.haMqttClient()` freigelegt — das Produkt kann HA-Entitäten durch sie veröffentlichen.

Das Gerät muss seine Client-ID setzen:

```cpp
intManager.setHaClientId(serialNumber);
```

## Moonraker / Klipper

`MoonrakerClient` verbindet sich über WebSocket (`ws://host:port/websocket`) und verwendet JSON-RPC 2.0, um Klipper-Objekte zu abonnieren.

Primärer Anwendungsfall — iHeater: Empfang der Kammer-Zieltemperatur über `gcode_macro VIRTUAL_CHAMBER`.

```json
{"type": "moonraker", "enabled": true, "host": "klipper.local", "port": 7125}
```

Der Client abonniert Klipper-Objekte einschließlich `gcode_macro VIRTUAL_CHAMBER`, `print_stats`, `display_status` und Temperatursensoren.

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

## Einschränkungen

- Eine aktive Integration zur Zeit. Das Umschalten ist atomar: die alte stoppt, die neue startet.
- Eine `BambuClient` Instanz pro Gerät (Singleton über einen statischen Zeiger).
- `LinkIntegrationsStore` speichert Konfiguration in NVS — die Einstellungen bleiben über Neustarts erhalten.
- Das Gerät muss seinen Typ spezifizieren (`setDeviceType`) für die korrekte Bambu-Modusauswahl:
  ```cpp
  intManager.setDeviceType(UartDeviceType::Dryer); // or Heater, IHeaterLink
  ```
