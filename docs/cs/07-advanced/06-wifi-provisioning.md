# Připojení k Wi-Fi a režim nastavení

Zařízení dostává heslo k síti dvěma cestami: po kabelu z webového instalátoru (protokol Improv) a vzduchem z mobilní aplikace **iDryer** pro Android a iOS (protokol ESPTouch v2). Poté heslo leží v NVS a přežije restarty.

Stará se o to `EspTouchProvisioner` (`platform/arduino/`). Produkt nemusí volat nic — `Link` ho spustí a řídí sám; callbacky slouží jen k zobrazení průběhu nastavení na displeji.

Verze protokolu nejsou kompatibilní: aplikace musí posílat ESPTouch **v2**. V první verzi se data přenášela délkami rámců a ty se lámaly, jakmile router přeposlal broadcast mezi pásmy 2,4 a 5 GHz — jedna mesh síť se společným názvem fungovala jen obden.

Rozumět tomuto chování se vyplatí z jednoho důvodu: zařízení má obnovit spojení tiše, pokud to jde, a volat majitele jen tehdy, když to bez něj nejde. Zmýlit se lze na obě strany — buď obtěžovat majitele při každém zaškobrtnutí routeru, nebo mlčky viset offline se špatným heslem.

## Čtyři fáze

Provisioner žije **jen do prvního úspěšného připojení**. `Link::loop()` ho volá, dokud Wi-Fi neběží; po prvním `WL_CONNECTED` přebírá řízení cloudový automat a režim nastavení se už nikdy nespustí — až do dalšího restartu.

### Fáze 1. Start, heslo je v paměti

Vytrvalý režim, bez diagnostiky.

Každých `kBootRetryMs` (6 sekund) nový pokus o připojení. Takto po dobu `kConnectFallbackMs` (90 sekund). Připojeno — pracujeme; nepřipojeno — spouštíme režim nastavení.

Proč bez diagnostiky: heslo není nové, někdy fungovalo. Nemá smysl zjišťovat, zda je slabý signál nebo se změnilo heslo — pokud v daném okně spojení není, zavoláme majitele a ten to na místě vyřeší.

Proč jsou opakování nutná: `WiFi.begin()` se volá jednou, z `Link::begin()`. Cloudový automat před `WL_CONNECTED` neběží a automatické opakování Arduina nepokrývá `reason` 202 a 205. Bez opakování je pokus přesně jeden a při slabém signálu jde zařízení do nastavení s naprosto platným heslem v paměti.

Proč 90 sekund — počítáno od nejhoršího případu, kdy se vrátí proud a všechno naskakuje najednou:

- **až 60 sekund** — start routeru. Oborový požadavek Broadband Forum, [TR-124 Issue 9](https://rg-device-requirements.broadband-forum.org/), bod `GEN.OPS.9`: „The RG MUST complete power up in 60 seconds or less". Výhrada: požadavek se týká úplného startu brány, samostatný limit pro objevení SSID v něm není. Měření na běžném hardwaru dávají 34–49 sekund do vysílání, takže 60 je rozumná horní mez, nikoli průměr;
- **plus naše vlastní připojení** — obvykle 2–5 sekund, ale při `RSSI` kolem −85 dBm se protáhne na dvacet.

Odtud 90: šedesát na router a třicet na nás. Méně riskuje, že zavoláme majitele právě ve chvíli, kdy se síť objeví; výrazně více nechá majitele příliš dlouho hledět na zařízení bez spojení a bez vysvětlení.

Router a přístupový bod zapnuté společně startují paralelně, ne po sobě — jejich časy se nesčítají. Při přísnějším kritériu „klient dostal adresu a má internet" jde počet na 90–120 sekund: tam se přidává WAN, DHCP nebo PPPoE a synchronizace modemu. Na to čekat nemusíme — zařízení stačí dostat se do lokální sítě.

### Fáze 2. Start, heslo v paměti není

Režim nastavení se spustí okamžitě — není na co čekat.

### Fáze 3. Heslo právě dorazilo

Chytrý režim, `driveConnect()`.

Zde je heslo **čerstvé a může být špatné** — majitel se mohl překlepnout. Provisioner proto rozlišuje příčiny selhání:

- počítá po sobě jdoucí selhání autentizace (`kAuthFailLimit`, 8);
- po pátém prohlédne éter a zapamatuje si nejlepší `RSSI`;
- je-li `RSSI` horší než `kWeakRssi` (−80 dBm), návrat se ruší: tytéž `reason` 15 a 202 vznikají i ztrátou auth rámců při správném hesle a připojení může trvat minuty. Tam platí jiná lhůta — `kWeakSignalGiveUpMs` (10 minut);
- při silném signálu a osmi selháních se vrací do režimu nastavení: jinak z překlepu není cesta ven bez přeflashování.

K tomu hlídač úplného ticha (`kWatchdogMs`, 20 sekund): když nepřicházejí ani události, ani spojení, stack se šťouchne pomocí `esp_wifi_disconnect()`.

### Fáze 4. Spojení se ztratilo za provozu

Provisioner se už nevolá. Spojení tiše obnoví cloudový automat, režim nastavení se nespustí — majitele nerušíme. Z pohledu zařízení jsou „router byl na noc vypnut" a „router byl vyměněn" nerozlišitelné a hádat se o to nepokouší.

## Jak zařízení říct, že se síť změnila

Restartovat.

Po restartu začíná fáze 1: staré heslo nesedí, 90 sekund uplyne naprázdno, spustí se režim nastavení — a nové heslo lze poslat z telefonu nebo zapojit kabel. Žádné heuristiky, prosté vypnout a zapnout.

## Vlastnost: z režimu nastavení se síť sama nechytí

Dokud zařízení poslouchá éter, **nepokouší se** připojit k uložené síti: rádio je obsazené příjmem a pokusy o připojení ustávají, dokud nedorazí nové heslo.

Praktický důsledek: pokud routeru trvalo naskočení déle než 90 sekund a zařízení už přešlo do nastavení, samo se do sítě nevrátí, ani když se síť objeví. Buď poslat heslo, nebo zařízení restartovat.

Je to vlastnost režimu, ne opomenutí: poslouchat éter a zároveň navazovat spojení nelze.

## Časové limity

| Konstanta | Hodnota | Co určuje |
|---|---|---|
| `kBootRetryMs` | 6 s | perioda opakovaného pokusu ve fázi 1 |
| `kConnectFallbackMs` | 90 s | jak dlouho zkoušíme, než zavoláme majitele |
| `kReconnectDelayMs` | 700 ms | pauza po výpadku před dalším pokusem (fáze 3) |
| `kWatchdogMs` | 20 s | ticho ve stacku, po kterém se šťouchne |
| `kAuthFailLimit` | 8 | selhání autentizace v řadě do návratu do nastavení |
| `kWeakRssi` | −80 dBm | hranice „slabého signálu" |
| `kWeakSignalGiveUpMs` | 10 min | jak dlouho snášíme slabý signál, než to vzdáme |
| `kRestartMs` | 10 min | restart režimu nastavení, pokud heslo nikdy nedorazí |

## Co vidí majitel

Vstup do režimu nastavení vyvolá callback `Notice`:

- `Listening` — běžné nastavení: buď v paměti není síť, nebo spojení už fungovalo;
- `CheckPassword` — síť je zadaná, ale zařízení ani jednou nepustila. Jediné zbývající vysvětlení je špatné heslo.

Jak to zobrazit, rozhoduje produkt. V `idryer-touch` je to celoobrazovkové hlášení.
