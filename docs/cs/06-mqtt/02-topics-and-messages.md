# MQTT témata a zprávy

Všechna témata mají formu `idryer/{serial}/{suffix}`, kde `{serial}` je sériové číslo zařízení.

Tento dokument popisuje témata a příkazy implementované pomocí `MqttClient` z `idryer-core`. Úplné rozhraní platformy (všechny příkazy backendu pro všechny typy zařízení) je v `contracts/portal_backend_status.md`.

## Zařízení → backend

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

Publikováno jednou, když se zařízení poprvé přejde do stavu Online, a znovu při příjmu příkazu `ping`.

Payload je definován produktem prostřednictvím `IProfile::buildInfoJson()`. Pole očekávaná backendem minimálně: `hardwareVersion`, `firmwareVersion`, `timestamp`.

Příklad pro Storage Link:

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

### telemetry

```
idryer/{serial}/telemetry    retained=false    interval ~10 s
```

Publikováno produktem prostřednictvím `pub.publishTelemetry()`. Knihovna nepublikuje automaticky.

Příklad pro Storage Link (senzor klimatu):

```json
{
  "units": [
    {"unitId": "U1", "temperature": 23.5, "humidity": 47.2}
  ]
}
```

### status

```
idryer/{serial}/status    retained=true    published on change
```

Publikováno produktem při změně stavu prostřednictvím `pub.publishStatus()`. Payload je definován produktem.

### config

```
idryer/{serial}/config    retained=false    on request
```

Publikováno po přijetí `device.getConfig` (invoke) nebo v odpovědi na `get_config`. Voláno prostřednictvím `pub.publishConfig()` nebo `pub.publishConfigRaw()`.

Pro velké payload (> 16000 bajtů) publikováno v blocích: každý blok obsahuje `tid`, `idx`, `total`, `last`, `d`.

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

Částečná aktualizace konfigurace prostřednictvím `pub.publishConfigDelta()`. Backend očekává pole `d` (objekt se změnami).

### events

```
idryer/{serial}/events    retained=false    on event
```

Publikováno produktem prostřednictvím `pub.publishEvent()`. Knihovna negeneruje události automaticky.

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

Publikováno pomocí `LinkIntegrationsManager`. Obsahuje stav aktivního připojení integrace.

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

Nastaveno brokerem automaticky, když TCP spojení padne. Zařízení nikdy tento typ manuálně nepublikuje.

## Backend → zařízení

Zařízení se přihlašuje k tématu `idryer/{serial}/commands/#`.

### commands/ping

```
idryer/{serial}/commands/ping
```

Zpracováno přímo pomocí `IdryerRuntime` — synchronizuje systémový čas prostřednictvím `settimeofday()` a znovu publikuje info.

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

Preferovaná cesta pro akce produktu. Knihovna předá příkaz `CommandHandler` produktu (doporučená cesta). Pokud není `CommandHandler` zaregistrován, příkaz přejde na vestavěný `ActionDispatcher` (fallback).

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

Vestavěná akce `device.getConfig` je zpracována runtimem nebo handlerem produktu — volá `IProfile::getConfig()` a publikuje výsledek.

### commands/set

```
idryer/{serial}/commands/set
```

Nastaví jeden parametr konfigurace. Předán `CommandHandler` produktu (doporučená cesta). Fallback — vestavěný `ActionDispatcher::handleSet()`, pokud není `CommandHandler` zaregistrován.

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

Správa integrací. Zpracováno pomocí `LinkIntegrationsManager` prostřednictvím `CommandHandler` produktu.

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```

### commands/bambu_apply

```
idryer/{serial}/commands/bambu_apply
```

Aplikuj parametry filamentu na slot AMS tiskárny Bambu. Zpracováno pomocí `LinkIntegrationsManager`.

### Ostatní příkazy platformy

Příkazy `drying`, `storage`, `profile`, `stop`, `get_config`, `read_rfid`, `write_rfid` a další jsou součástí úplného rozhraní platformy iDryer. Nejsou zpracovány `idryer-core` přímo; jsou doručeny `CommandHandler` produktu. Odkaz: `contracts/portal_backend_status.md`.

## Formát tématu

```c
// Konstrukce tématu
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

Konstanty suffixu jsou definovány v `mqtt/idryer_topics.h`:

```c
#define IDRYER_TOPIC_INFO               "info"
#define IDRYER_TOPIC_TELEMETRY          "telemetry"
#define IDRYER_TOPIC_STATUS             "status"
#define IDRYER_TOPIC_CONFIG             "config"
#define IDRYER_TOPIC_CONFIG_DELTA       "config/delta"
#define IDRYER_TOPIC_EVENTS             "events"
#define IDRYER_TOPIC_OFFLINE            "offline"
#define IDRYER_TOPIC_INTEGRATIONS_STATUS "integrations/status"
#define IDRYER_TOPIC_CMD_WILDCARD       "commands/#"
```
