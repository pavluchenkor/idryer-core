# WLAN-Verbindung und Einrichtungsmodus

Das Gerät erhält das Netzwerkpasswort auf zwei Wegen: über Kabel aus dem Web-Installer (Improv-Protokoll) und über die Luft aus der **iDryer**-App für Android und iOS (ESPTouch-v2-Protokoll). Danach liegt das Passwort im NVS und übersteht Neustarts.

Zuständig ist `EspTouchProvisioner` (`platform/arduino/`). Das Produkt ruft nichts auf — `Link` startet und betreibt ihn selbst; die Callbacks dienen nur dazu, den Verlauf der Einrichtung auf einem Display zu zeigen.

Die Protokollversionen sind nicht kompatibel: Die App muss ESPTouch **v2** senden. In der ersten Version wurden die Daten über Framelängen übertragen, und die brachen, sobald ein Router den Broadcast zwischen dem 2,4- und dem 5-GHz-Band weiterreichte — ein einzelnes Mesh-Netz mit gemeinsamem Namen funktionierte nur jedes zweite Mal.

Dieses Verhalten zu verstehen lohnt sich aus einem Grund: Das Gerät soll die Verbindung still wiederherstellen, wenn es das kann, und den Besitzer nur dann rufen, wenn es ohne ihn nicht geht. Man kann in beide Richtungen danebenliegen — entweder bei jedem Schluckauf des Routers stören oder mit falschem Passwort stumm offline hängen.

## Vier Phasen

Der Provisioner lebt **nur bis zur ersten erfolgreichen Verbindung**. `Link::loop()` ruft ihn auf, solange kein WLAN steht; nach dem ersten `WL_CONNECTED` übernimmt die Cloud-Zustandsmaschine, und der Einrichtungsmodus startet nie wieder — bis zum nächsten Neustart.

### Phase 1. Start, Passwort ist gespeichert

Hartnäckiger Modus, ohne Diagnose.

Alle `kBootRetryMs` (6 Sekunden) ein neuer Verbindungsversuch. Das geht `kConnectFallbackMs` (90 Sekunden) lang. Verbunden — wir arbeiten; nicht verbunden — der Einrichtungsmodus startet.

Warum ohne Diagnose: Das Passwort ist nicht neu, es hat irgendwann funktioniert. Es lohnt nicht herauszufinden, ob das Signal schwach ist oder das Passwort geändert wurde — gibt es im Zeitfenster keine Verbindung, rufen wir den Besitzer, und der klärt es vor Ort.

Warum die Wiederholungen nötig sind: `WiFi.begin()` wird genau einmal aufgerufen, aus `Link::begin()`. Die Cloud-Zustandsmaschine läuft vor `WL_CONNECTED` nicht, und der Auto-Reconnect von Arduino deckt `reason` 202 und 205 nicht ab. Ohne Wiederholungen gibt es genau einen Versuch, und bei schwachem Signal geht das Gerät mit einem völlig gültigen Passwort im Speicher in den Einrichtungsmodus.

Warum 90 Sekunden — gerechnet vom schlechtesten Fall, wenn der Strom wiederkommt und alles gleichzeitig hochfährt:

- **bis zu 60 Sekunden** — der Router-Start. Branchenanforderung des Broadband Forum, [TR-124 Issue 9](https://rg-device-requirements.broadband-forum.org/), Punkt `GEN.OPS.9`: „The RG MUST complete power up in 60 seconds or less". Einschränkung: Die Anforderung betrifft den vollständigen Start des Gateways, eine eigene Grenze für das Erscheinen der SSID gibt es dort nicht. Messungen an Massenhardware ergeben 34–49 Sekunden bis zum Funkbetrieb, 60 ist also eine sinnvolle Obergrenze und kein Mittelwert;
- **plus unsere eigene Verbindung** — normalerweise 2–5 Sekunden, bei einem `RSSI` um −85 dBm jedoch bis zu zwanzig.

Daher 90: sechzig für den Router und dreißig für uns. Weniger riskiert, den Besitzer genau dann zu rufen, wenn das Netz gleich erscheint; deutlich mehr lässt ihn zu lange auf ein Gerät ohne Verbindung und ohne Erklärung schauen.

Router und Access Point, die gemeinsam eingeschaltet werden, starten parallel und nicht nacheinander — ihre Zeiten dürfen nicht addiert werden. Legt man das strengere Kriterium an — „der Client hat eine Adresse und erreicht das Internet" —, landet man bei 90–120 Sekunden: Dort kommen WAN, DHCP oder PPPoE und die Modem-Synchronisation hinzu. Darauf müssen wir nicht warten — das Gerät muss nur ins lokale Netz.

### Phase 2. Start, kein Passwort gespeichert

Der Einrichtungsmodus startet sofort — es gibt nichts abzuwarten.

### Phase 3. Das Passwort ist gerade eingetroffen

Intelligenter Modus, `driveConnect()`.

Hier ist das Passwort **frisch und möglicherweise falsch** — der Besitzer kann sich vertippt haben. Deshalb unterscheidet der Provisioner die Fehlerursachen:

- er zählt aufeinanderfolgende Authentifizierungsfehler (`kAuthFailLimit`, 8);
- nach dem fünften scannt er die Luft und merkt sich den besten `RSSI`;
- ist der `RSSI` schlechter als `kWeakRssi` (−80 dBm), wird der Rücksprung abgebrochen: Dieselben `reason` 15 und 202 entstehen auch durch verlorene Auth-Frames bei korrektem Passwort, und die Verbindung kann Minuten dauern. Dort gilt eine andere Frist — `kWeakSignalGiveUpMs` (10 Minuten);
- bei kräftigem Signal und acht Fehlversuchen kehrt er in den Einrichtungsmodus zurück: Sonst gibt es aus einem Tippfehler keinen Ausweg außer Neuflashen.

Dazu ein Watchdog für völlige Stille (`kWatchdogMs`, 20 Sekunden): Kommen weder Ereignisse noch eine Verbindung, wird der Stack mit `esp_wifi_disconnect()` angestoßen.

### Phase 4. Die Verbindung bricht im Betrieb ab

Der Provisioner wird nicht mehr aufgerufen. Die Cloud-Zustandsmaschine stellt die Verbindung still wieder her, der Einrichtungsmodus startet nicht — der Besitzer wird nicht gestört. Aus Sicht des Geräts sind „der Router wurde für die Nacht ausgeschaltet" und „der Router wurde ausgetauscht" nicht unterscheidbar, und es versucht gar nicht erst zu raten.

## Wie man dem Gerät sagt, dass sich das Netz geändert hat

Neu starten.

Nach dem Neustart beginnt Phase 1: Das alte Passwort passt nicht, 90 Sekunden vergehen vergeblich, der Einrichtungsmodus startet — und ein neues Passwort kann per Telefon oder über Kabel geschickt werden. Keine Heuristik, einfach aus- und wieder einschalten.

## Eine Eigenschaft: aus dem Einrichtungsmodus wird das Netz nicht selbst aufgegriffen

Solange das Gerät die Luft abhört, versucht es **nicht**, sich mit dem gespeicherten Netz zu verbinden: Das Funkmodul ist mit dem Empfang belegt, und Verbindungsversuche ruhen, bis ein neues Passwort eintrifft.

Praktische Folge: Brauchte der Router länger als 90 Sekunden und das Gerät ist bereits im Einrichtungsmodus, kehrt es von allein nicht ins Netz zurück, auch wenn das Netz wieder da ist. Entweder das Passwort schicken oder das Gerät neu starten.

Das ist eine Eigenschaft des Modus und kein Versäumnis: Die Luft abhören und gleichzeitig eine Verbindung aufbauen geht nicht.

## Zeitgrenzen

| Konstante | Wert | Wofür sie steht |
|---|---|---|
| `kBootRetryMs` | 6 s | Wiederholungsabstand in Phase 1 |
| `kConnectFallbackMs` | 90 s | wie lange wir es versuchen, bevor wir den Besitzer rufen |
| `kReconnectDelayMs` | 700 ms | Pause nach einem Abbruch vor dem nächsten Versuch (Phase 3) |
| `kWatchdogMs` | 20 s | Stille im Stack, nach der er angestoßen wird |
| `kAuthFailLimit` | 8 | Auth-Fehler in Folge bis zur Rückkehr in die Einrichtung |
| `kWeakRssi` | −80 dBm | Schwelle für „schwaches Signal" |
| `kWeakSignalGiveUpMs` | 10 min | wie lange wir ein schwaches Signal aushalten, bevor wir aufgeben |
| `kRestartMs` | 10 min | Neustart des Einrichtungsmodus, wenn nie ein Passwort eintrifft |

## Was der Besitzer sieht

Der Eintritt in den Einrichtungsmodus löst den `Notice`-Callback aus:

- `Listening` — gewöhnliche Einrichtung: entweder ist kein Netz gespeichert, oder die Verbindung stand bereits;
- `CheckPassword` — das Netz ist eingetragen, hat das Gerät aber nie hereingelassen. Die einzige verbleibende Erklärung ist ein falsches Passwort.

Wie das dargestellt wird, entscheidet das Produkt. In `idryer-touch` ist es eine bildschirmfüllende Meldung.
