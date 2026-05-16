# Publikování přes devicePublisher

## Kdy použít

`iDryer::Link` už obsahuje dva vestavěné transports: MQTT (cloud) a Local WebSocket (LAN). Další transport není pro většinu úkolů potřeba.

Používej `s_link.devicePublisher()` když produkt sestavuje vlastní zátěž a musí ji poslat do obou kanálů současně — například když publikuješ konfiguraci nabídky v odpovědi na `commands/get_config`.

## Hotový kód

```cpp
// main.cpp (fragment)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// Publikuj libovolnou JSON zátěž do MQTT a Local WS jediným voláním.
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // funkce produktu
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

Jediné volání `publishConfigRaw` dodělá zátěž do MQTT tématu `idryer/{serial}/config` a všem aktivním LAN WS klientům. Není třeba vytvářet další klienty nebo témata.

## Vysvětlení

`devicePublisher()` je pomocník duální publikace fasády. Používej jej místo přímého volání `mqttClient()` nebo `LocalAccess`, pokud nepotřebuješ publikovat na nestandartní téma.

Telemetrie a stav se publikují automaticky fasádou na časovači — `devicePublisher()` pro ně není potřeba.

## Když je potřeba třetí transport

Přidání třetího kanálu (BLE, Serial JSON, UART proxy) je architektonické rozšíření fasády, ne návod. Drtivá většina zařízení to nepotřebuje.

Pokud to opravdu potřebuješ — vstupní body jsou v `lib/idryer-core/src/cloud/` (cloud stavový stroj, MQTT) a `lib/idryer-core/src/` (místní přístup). Než budeš pokračovat, potvrď že vestavěné MQTT a Local WS nejsou pro tvůj případ dostatečné.

## Plný příklad v repo

`publishFullMenu()` v `iDryer-Storage/src/main.cpp:171` — publikování úplné JSON nabídky přes `s_link.devicePublisher()->publishConfigRaw(buf, len)`.
