# Spárování s účtem

Spárování je jednorázový postup: zařízení dostane jednorázový párovací token, vymění ho na portálu za trvalé tajemství a uloží tajemství do NVS. Pak se po každém restartu připojuje k portálu samo. Dokud tajemství nemá, je zařízení v režimu nastavení a čeká na token.

## V aplikaci iDryer

1. Zařízení je v síti (viz [Wi-Fi](01-wifi.md)), telefon ve stejné síti.
2. **Připojit nové zařízení** → krok **Spárování** (pokud už je zařízení v síti, klepněte na čip kroku nahoře v okně) → **Spárovat**.
3. Aplikace najde zařízení v místní síti, získá od portálu token, předá ho zařízení a počká, až portál potvrdí, že je zařízení online.
4. Po zprávě **Zařízení spárováno** je zařízení v seznamu na portálu i v aplikaci.

Log před spárováním:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Po něm:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

Webový instalátor firmwaru předává token přes USB příkazem `PAIR_TOKEN` (viz níže).

## Odpojení

Odpojení v aplikaci nebo na portálu dorazí do zařízení jako příkaz `revoke`. Každý firmware potřebuje obsluhu:

```cpp
// Odpojení v aplikaci nebo na portálu: smazat tajemství a čekat na nové spárování.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` smaže tajemství a síť ponechá: zařízení znovu čeká na spárování. Bez obsluhy tajemství v zařízení zůstane a znovu spárovat ho půjde až po `WIPE_IDENTITY`.

## Příkazy přes USB

Po připojení k síti přijímá jádro řádky na sériovém portu (115200, každý řádek končí znakem nového řádku):

| Příkaz | Odpověď | Co dělá |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | stav: `state` je `bound` (spárováno) nebo `setup` (čeká na token), `cloud` je `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | smaže tajemství jako `revoke`; síť zůstane |
| `PAIR_TOKEN:<token>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | předá párovací token; spárované zařízení token nepřijme |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Než se zařízení připojí k síti, port drží Improv a příkazy neodpovídají. V sestavení s `IDRYER_DEV_REPL` patří port produktu a tyto příkazy neexistují: viz [Podrobné nastavení](99-detailed-setup.md).

## Když to nevyšlo

- aplikace zařízení nenajde: telefon a zařízení ve stejné síti; sítě pro hosty často blokují vyhledávání zařízení;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`, nebo zařízení patří jinému účtu: odpojte ho na portálu nebo pošlete `WIPE_IDENTITY` a spárujte znovu.

## Dál

[Telemetrie](03-telemetry.md).
