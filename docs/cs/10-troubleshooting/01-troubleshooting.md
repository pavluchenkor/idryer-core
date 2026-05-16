# Řešení problémů

Běžné příznaky při práci s `idryer-core`, jejich příčiny a řešení.

Předtím, než budete číst, ujistěte se, že jsou povoleny protokoly HAL (`idryer::hal::initArduinoHal(&Serial)`) a že je v `platformio.ini` nastaveno `-DCORE_DEBUG_LEVEL=3` nebo vyšší.

## WiFi

### Stavový stroj zaseknutý v `WifiConnecting`

Příznaky: protokol opakuje `state: WifiConnecting`, přechod na `Provisioning` nikdy nenastane.

Možné příčiny:

- Nesprávné SSID/heslo. Zkontrolujte `WIFI_SSID` / `WIFI_PASSWORD` v `secrets.h`. Po zřizování Improv přichází přihlašovací údaje z NVS, ne z `secrets.h`.
- Síť 5 GHz. ESP32 podporuje pouze 2,4 GHz.
- Skrytá síť nebo filtr MAC na směrovači.
- `WiFi.begin()` volán před `idryer::hal::initArduinoHal(...)` — žádný výstup protokolu, ale to není příčinou zacyklení, jen slepota.

Co zkontrolovat:

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFi se připojí, ale vypadne po 30–60 sekundách

Typicky: slabý signál (`RSSI < -80 dBm`), ESP32-C3 napájen ze střediska USB bez vyhrazené dodávky 5V/1A, konflikt s úkoly FreeRTOS.

Protokolujte RSSI v loop produktu:

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## Zřizování a požadavky

### Stavový stroj zaseknutý v `Provisioning`

Příznaky: `state: Provisioning` bez přechodu na `Registering` nebo `AwaitingClaim`.

Příčiny:

- Nesprávné `IDRYER_API_BASE` v build_flags. Musí být `https://portal.idryer.org/api` (produkce) nebo `https://staging.idryer.org/api` (staging).
- Chybí certifikát TLS (Let's Encrypt ISRG Root X1). Vloženo v `root_ca.h`, ale když se staví bez `MQTT_USE_TLS`, klient HTTP také používá TLS — certifikát root CA je potřebný také pro HTTP API.
- Čas zařízení není synchronizován (handshake TLS vyžaduje platný den). Zkontrolujte, že `configTime(...)` je volán v `setStateChangeCallback` po prvním výstupu z `WifiConnecting` (jako v Storage Link).

### Stavový stroj zaseknutý v `AwaitingClaim`

Toto je normální stav, když uživatel v portálu nezadal PIN. PIN je vytištěn do protokolu přes `setClaimPinCallback`.

Pro automatické požadavky (samostatné zařízení bez UI):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Po `requestClaim()` backend vydá PIN, který musí uživatel zadat v portálu.

### `seedSerialFromMac()` vygeneroval sériové číslo, ale v portálu bylo zadáno jiné

Sériové číslo uložené v NVS má přednost před generováním MAC. `seedSerialFromMac()` píše do NVS pouze pokud ještě neexistuje sériové číslo. Chcete-li změnit sériové číslo, vymažte NVS:

```cpp
s_credentials.clear();
```

## MQTT

### Stavový stroj vstoupil do `MqttConnecting`, ale nedosáhne `Online`

Příčiny:

- Broker nedosažitelný. Produkce: `mqtt.idryer.org:8883`, staging: `staging.idryer.org:1884`.
- `MQTT_USE_TLS=1` bez správného certifikátu root CA — handshake tiše selhá.
- `setBufferSize(16384)` není aplikován — buffer `PubSubClient` je ve výchozím nastavení 256 bajtů. `MqttClient` již nastavuje 16384, ale pokud používáte `PubSubClient` přímo — nastavte buffer sami.
- Trvalá relace "zaseknutá" na brokeru s jiným ID klienta. Vymažte NVS a znovu programujte.

### Příkazy z backendu nepřicházejí

Zkontrolujte odběr — `MqttClient` se přihlásí k `idryer/{serial}/commands/#` s QoS 1. Pokud se odběr nezdařil, protokol bude zobrazovat:

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

Ověřte, že `setCommandHandler()` je volán **před** `runtime.begin()` — jinak by byl první dávka příkazů zmeškán.

### `PubSubClient` se odpojí přesně v 60sekundových intervalech

Toto je keep-alive timeout. Váš MQTT loop nemusí být volán dostatečně často — `s_runtime.loop()` se musí točit bez dlouhých bloků. Zkontrolujte, že `loop()` nemá `delay(>500ms)` a žádné blokování síťových volání.

## Příkazy a obslužné programy

### `commands/invoke` přichází, ale `ActionDispatcher` není volán

Pokud jste zaregistrovali `setCommandHandler()`, **vestavěný fallback na `ActionDispatcher` je zakázán**. `IdryerRuntime` předá vše (kromě `ping`) vašemu `CommandHandler`. Musíte explicitně volat `s_dispatcher.handleInvoke(data)` tam za příkazy `invoke`.

Šablona:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... příkazy produktu ...
}
```

### `commands/set` přijat, ale konfigurace nebyla aplikována

`ActionDispatcher::handleSet` ekstrahuje `id` a `val` a předá je registrované `SetCallback`. Zkontrolujte:

- `dispatcher.setSetCallback(onSetCommand, nullptr)` je volán v `setup()`.
- `onSetCommand` skutečně volá `s_profile.applyConfig(id, val)`.
- `applyConfig` vrací `true` pro známé hodnoty `id`. Pro neznámé vrací `false` a změny jsou ignorovány.

## Telemetrie

### Telemetrie není publikována

`idryer-core` nepublikuje telemetrii automaticky. Kód produktu to vždy dělá.

Zkontrolujte:

- `pub.publishTelemetry(doc)` (nebo `s_mqtt.publishTelemetry(doc)` pokud se LocalAccess nepoužívá) je skutečně volán v `loop()`.
- Podmínka sazby neblokuje všechna volání. Běžná chyba:
  ```cpp
  if (millis() - lastTm > 10000) { /* publikuj */ }
  ```
  Na prvním průchodu je `lastTm == 0` a `millis()` je stále malý — větev se nikdy nespustí. Použijte `>=` a inicializujte `lastTm` na prvním průchodu.
- `s_runtime.isOnline() == true`. MQTT je odpojeno před Online — publikování nepůjde.
- Velikost `JsonDocument` je dostatečná pro datovou část. Zkontrolujte `doc.overflowed()` po `serializeJson`.

### `publishTelemetry` vrací `false`

Příčiny:

- Nepřipojeno k brokeru (`MqttClient::isConnected() == false`).
- Buffer překročen — datová část větší než `MQTT_BUFFER_SIZE` (16384 bajtů). Pro velká data použijte `publishConfigRaw` (s fragmenty) nebo snižte datovou část.

### `DevicePublisher::publishTelemetry` nedosáhne klienta WS

`DevicePublisher` neparameterizuje chybu, pokud není klient WS připojen — jednoduše přeskočí část WS. Zkontrolujte `s_local.isClientConnected()`. Pokud je `false` — klient není ověřen nebo není připojen.

## NTP a systémový čas

### Čas zařízení není synchronizován

Synchronizace NTP se spouští v `setStateChangeCallback` po prvním výstupu z `WifiConnecting`:

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

Pokud není toto zpětné volání zaregistrováno — čas není synchronizován automaticky. Handshake TLS k brokeru vyžaduje platný čas; jinak je certifikát považován za vypršený nebo z budoucnosti.

Alternativní kanál: `IdryerRuntime` zpracovává `commands/ping` a aplikuje `data["timestamp"]` přes `settimeofday()`. Pokud backend posílá ping jednou za minutu — čas se aktualizuje bez NTP.

### Handshake TLS selhává po dlouhém provozu

Pokud je server NTP nedosažitelný a zařízení běží bez restartování dlouhou dobu, čas se může posunout (zejména na ESP32-C3 bez TCXO). Příznak: náhlé `connection failed` po několika dnech provozu.

Řešení: ujistěte se, že `pool.ntp.org` je dosažitelný z vaší sítě, nebo přijímejte `commands/ping` z backendu nejčastěji.

### `getIsoTimestamp` vrací rok 1970

Systémový čas dosud není synchronizován. Čas se objeví po prvním úspěšném `configTime` nebo `commands/ping`. Až do té doby budou `info`/`telemetry` publikovány s záložníkem.

## ArduinoJson

### Chyba kompilace: `StaticJsonDocument` není členem `ArduinoJson`

Používáte ArduinoJson v7. Typ `StaticJsonDocument` existuje pouze v6. Řešení:

- Zapiňte v6 v `platformio.ini`:
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- Nebo migrujte svůj kód do API v7 (`JsonDocument` místo `StaticJsonDocument<N>`). `idryer-core` je napsán pro v6.

### Chyba kompilace: nejasné přetížení nebo neshoda typu

Dvě verze ArduinoJson se mohou nacházet v jednom projektu prostřednictvím přechodných závislostí. Zkontrolujte:

```bash
pio pkg list -e my-device | grep -i arduinojson
```

Musí existovat **jedna** verze. Pokud jsou dvě — zapiňte ji explicitně přes `lib_deps` a v případě potřeby přes `lib_ldf_mode = chain+` nebo `lib_ignore`.

### `doc.overflowed()` true po serializeJson

Velikost `StaticJsonDocument<N>` je příliš malá pro datovou část. Zvětšete `N` nebo použijte `DynamicJsonDocument` pro zřídka volané cesty.

## Místní WS (LocalAccess)

### Aplikace nenalezne zařízení v síti LAN

mDNS by měl začít **ihned poté, co je sériové číslo dostupné** přes `s_local.initMdns(serial)`. Zkontrolujte:

- Směrovač neblokuje multicast.
- Aplikace hledá `_idryer._tcp` na portu 81.
- Sériové číslo zařízení odpovídá registrovanému v portálu.

### Klient WS připojen, ale přijímá `auth_required`

První zpráva od klienta musí být `{"type":"auth","token":"<device_token>"}`. Pokud je token neplatný, `LocalAccess` volá `setTokenRefreshCallback()`. Produkt v tomto zpětném volání musí znovu přečíst token z `ICredentialStore` a volat `s_local.updateToken(...)`.

## Paměť a stabilita

### Volná halda se časem snižuje

`PubSubClient::loop()` a `WebSocketsServer::loop()` by neměly unést, ale zkontrolujte svůj kód produktu:

- Vytvořte `JsonDocument` na stacku (`StaticJsonDocument<N>`), ne na haldě (`DynamicJsonDocument`) pro často volané cesty.
- `String` v kódu produktu na ESP32-C3 rychle fragmentuje haldu — použijte `char[]` a `snprintf`.

### `Stack overflow` nebo `Guru Meditation`

`s_runtime.loop()` nespouští úkoly FreeRTOS — vše běží v smyčce Arduino. Pokud dojde k selhání stacku, hledejte:

- Velké místní `JsonDocument`/`char[8192]` na stacku smyčky Arduino (výchozí 8 KB).
- Hlubokou rekurzi v kódu produktu.

Zvětšete stack smyčky Arduino:

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi (zřizování přes Serial)

### Improv nepřijímá přihlašovací údaje

Improv musí vlastnit `Serial`, dokud nejsou přijaty přihlašovací údaje:

```cpp
idryer::hal::initArduinoHal(nullptr);   // protokoly na /dev/null, zatímco Improv drží Serial
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // obnovit výstup protokolu
}
```

Pokud `HAL_LOG_*` píše na `Serial` paralelně s protokolem Improv, Improv selže na checksum.

### Klient Improv nevidí zařízení

Zkontrolujte `ChipFamily` v `setDeviceInfo`. Musí odpovídat aktuálnímu čipu: `CF_ESP32_C3`, `CF_ESP32_S3`, `CF_ESP32_S2`, `CF_ESP32`. Neshoda — klient Improv nezobrazí zařízení v seznamu.

Také se ujistěte, že je baud sazba Serial 115200. Protokol Improv to očekává.

## Diagnostika integrace

### Úplný výstup diagnostiky (1 Hz)

Menu → `DIAGNOSTICS → DIAG LOG` (`menu.diag_en`). Ve výchozím nastavení vypnuto.
Povolte přes UI zařízení, portál (`commands/set` s `bind=diag_en`),
nebo REPL (`set diag_en 1`).

Když je povoleno, blok je vytištěn na Serial jednou za sekundu:

```
=========== iHeater Link diagnostika ===========
[device]    serial=DEVICE_... online=1 uptime=42s
[wifi]      status=3 ssid=Apart_4 ip=192.168.0.140 rssi=-51
[rmt-out]   mode=DRYING target=70.0°C
[active]    bambu
[bambu]     state=CONNECTED  ip=192.168.0.171 serial=<set> lan=<set>
            gcode_state='RUNNING' tray='PLA' chamber_target=0.0 chamber_temp=0.0
[moonraker] state=DISABLED   ws=ws://192.168.0.171:7125
            vc.available=0 vc.target=0.0 vc.temp=0.0 vc.has_sensor=0
[ha]        state=DISABLED   host=<empty>:1883 user=<empty>
[menu]      bambu_en=1 moon_en=0 ha_en=0 diag_en=1  mat_pla=45 ...
================================================
```

Užitečné pro vzdálené diagnózy: uživatel povolí `DIAG LOG`, zkopíruje výstup → viditelné jsou stavy konektorů, lastError a co se skutečně posílá na RMT.

### Kanál ANOMALY (na základě událostí)

Nezávisle na `diag_en` konektory a pomocníci píší samostatné řádky s
předponou `[!] ANOMALY` na neočekávané podmínky:

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

Předpona `[!]` vizuálně zvýrazní anomálii v obecném proudu protokolu. To je první věc, kterou je třeba hledat v Serial, když něco "nefunguje".

### Auto-OFF při ztrátě spojení (fail-safe)

Pokud aktivní integrace ztratí připojení (odpojení TCP/WS), konektor
okamžitě obnoví cílovou teplotu:

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`, `available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF.
- **Bambu** — přechod `Connected → !Connected` → `chamberTarget=0`, `trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF.
- **HA** — fail-safe zatím není implementován.

Bez této logiky by se topení pokračovalo na poslední známé cílové teplotě do doby obnovení spojení.

### Bambu: filtr gcode_state

`auto_heat` topí **pouze** když `gcode_state == "RUNNING"` nebo `"PREPARE"`.
Všechny ostatní stavy (`IDLE`, `FINISH`, `FAILED`, `PAUSE`, `INIT`, `OFFLINE`,
`SLICING`, `UNKNOWN`, prázdný) → OFF.

Při diagnostice věnujte pozornost `gcode_state` v řádku diagnostiky `[bambu]` — pokud zobrazuje `IDLE`/`FINISH`, topení nebude probíhat bez ohledu na to, zda je aktivní zásobník přítomen.

### Testovací lavice pro ladění bez tiskárny

Pro testování integrací bez skutečných tiskáren mohou obsahovat úložiště produktů
(např. iHeater-link) zástupné nástroje, jako jsou `fake_moonraker` / `fake_bambu`
které každých 30 sekund odešlou nárůst hodnot.
