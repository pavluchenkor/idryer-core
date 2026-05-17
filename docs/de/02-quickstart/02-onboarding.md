# Onboarding: erstes Geräteclaim

Onboarding ist ein einmaliges Verfahren, bei dem sich der ESP32 bei der iDryer Cloud registriert und an Ihr Konto beansprucht wird. Nach Abschluss wird das Gerät im Portal mit Status Online und Status Ready angezeigt, und alle nachfolgenden Startvorgänge sind automatisch.

## Was Sie benötigen

- Ein ESP32-Gerät, das mit einem REPL-Build geflasht ist: env `esp32c3-super-mini-dev` (siehe [In 5 Minuten beginnen](01-five-minutes.md)) oder beliebige Dev-Builds mit dem Flag `IDRYER_DEV_REPL=1`.
- USB-Kabel.
- Konto auf [portal.idryer.org](https://portal.idryer.org/) (für Entwicklung — [staging.idryer.org](https://staging.idryer.org/)).

## Pfad 1. Über Serial REPL (empfohlen)

Die REPL ist nur in Builds mit dem Flag `IDRYER_DEV_REPL=1` verfügbar. Öffnen Sie Serial Monitor, geben Sie drei Befehle ein — das Gerät verbindet sich mit WiFi, fordert eine PIN an und ist bereit zu beanspruchen.

### 1. Flashen Sie den Dev-Build

```bash
pio run -e esp32c3-super-mini-dev -t upload
```

Oder verwenden Sie beliebiges env, wo `-DIDRYER_DEV_REPL=1` gesetzt ist.

### 2. Öffnen Sie Serial Monitor

```bash
pio device monitor -b 115200
```

Nach dem Booten sehen Sie die Eingabeaufforderung:

```
[boot] iDryer dev REPL ready — type 'help'
```

Unmittelbar danach beginnen Cloud-Stack-Meldungen im Protokoll zu erscheinen:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```

### 3. WiFi verbinden

Geben Sie im Serial Monitor Console ein:

```
wifi MyHomeWiFi MySecretPass
```

Antwort:

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

Anmeldedaten werden im NVS geschrieben. Das Board ruft sofort `WiFi.begin()` auf. Das Protokoll zeigt:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

### 4. Holen Sie sich die PIN und beanspruchen Sie das Portal

Das Gerät stellt sich automatisch selbst bereit und registriert eine 7-stellige PIN. Die PIN ist 10 Minuten lang gültig.

1. Öffnen Sie [portal.idryer.org](https://portal.idryer.org/) (oder Staging).
2. Gehen Sie zu **Gerät hinzufügen**.
3. Geben Sie die PIN aus Serial Monitor ein.

Nach erfolgreichem Claim zeigt das Protokoll:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

Wenn die PIN abgelaufen ist, bevor Sie sie eingegeben haben — führen Sie den Befehl `claim` aus, um eine neue zu erhalten.

### Nützliche REPL-Befehle

| Befehl | Was er tut | Wann zu verwenden |
|---------|-------------|-------------|
| `help` | Befehlsliste anzeigen | Syntax merken |
| `status` | Aktueller Status: WiFi, IP, RSSI, online, Serial | Verbindungsdiagnostik |
| `wifi <ssid> <password>` | WiFi-Anmeldedaten im NVS speichern und neu verbinden | Erstes Onboarding oder Netzwerkänderung |
| `claim` | Manuell Claim-Fluss starten, neue PIN erhalten | PIN abgelaufen oder Neuanspruch erforderlich |
| `wipe` | NVS löschen (Anmeldedaten, Claim, Menü) und Neustart | Fabrikzurückstellung |
| `restart` | Software-Neustart der ESP | Schneller Neustart ohne physikalisches Trennen |

## Pfad 2. Über Improv-WiFi (Web Serial)

Improv-WiFi ist in alle Builds integriert und hängt nicht vom Flag `IDRYER_DEV_REPL` ab. Geeignet zum Übergeben eines Geräts an einen Benutzer oder wenn ein Terminal unpraktisch ist. Erfordert Chrome oder Edge — die Web Serial API wird in Safari oder Firefox nicht unterstützt.

### 1. Überprüfen Sie, dass das Board geflasht ist

Jeder Prod-Build ist OK. Improv-WiFi ist immer aktiv.

### 2. Öffnen Sie die Webseite

Gehen Sie zu [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/), klicken Sie auf **Verbinden** und wählen Sie den USB-Port des Geräts im Browser-Dialog.

### 3. Geben Sie SSID und Passwort ein

Die Seite fordert Name und Passwort des Netzwerks an, überträgt sie über Serial-Improv an das Board. Das Board speichert Anmeldedaten im NVS und verbindet sich mit WiFi. Provisioning und PIN-Abruf erfolgen automatisch — wie in Pfad 1.

!!! note
    Improv-WiFi kann `claim`, `wipe` nicht ausführen oder `status` nicht überprüfen. Verwenden Sie die REPL für den manuellen Claim-Fluss und die NVS-Verwaltung.

### Wann ist jeder Pfad zu verwenden

| Situation | Empfehlung |
|-----------|---------------|
| Eingebetteter Entwickler mit offener Konsole | REPL |
| Gerät an einen Benutzer übergeben | Improv-WiFi |
| Manuelles `wipe` oder Wiederholung `claim` erforderlich | REPL |
| Safari oder Firefox Browser | REPL |
| PlatformIO nicht installiert | Improv-WiFi |

## Wenn etwas schief gelaufen ist

**PIN nicht im Protokoll angezeigt.** Überprüfen Sie, dass das Gerät mit WiFi verbunden ist: Geben Sie `status` ein und überprüfen Sie, dass das Feld `ip=` in der Antwort nicht leer ist. Das Provisioning startet nicht ohne WiFi.

**PIN abgelaufen.** Geben Sie den Befehl `claim` ein — das Gerät fordert eine neue Registrierung an und gibt eine neue PIN aus.

**Gerät bereits an ein anderes Konto beansprucht.** Geben Sie `wipe` ein — NVS wird gelöscht, das Board wird neu gestartet und beginnt Onboarding von vorne.

**PIN wird vom Portal nicht akzeptiert.** Überprüfen Sie, dass Sie alle 7 Ziffern ohne Leerzeichen kopiert haben und dass weniger als 10 Minuten vergangen sind, seit die PIN angezeigt wurde.

**Improv-WiFi sieht das Gerät im Browser nicht.** Stellen Sie sicher, dass Sie Chrome oder Edge verwenden und dass der ESP32-USB-Treiber installiert ist.

## Was ist als Nächstes zu tun

- Vollständige Link-API: [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Sensor oder Peripheriegerät hinzufügen: [../04-patterns/](../04-patterns/)
