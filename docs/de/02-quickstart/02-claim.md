# Kopplung mit dem Konto

Die Kopplung ist ein einmaliger Vorgang: das Gerät bekommt ein einmaliges Kopplungstoken, tauscht es beim Portal gegen ein dauerhaftes Geheimnis und speichert das Geheimnis im NVS. Danach verbindet es sich nach jedem Neustart selbst mit dem Portal. Solange es kein Geheimnis hat, ist das Gerät im Einrichtungsmodus und wartet auf ein Token.

## In der iDryer-App

1. Das Gerät ist im Netz (siehe [WLAN](01-wifi.md)), das Telefon im selben Netz.
2. **Neues Gerät verbinden** → Schritt **Kopplung** (ist das Gerät schon im Netz, oben im Fenster auf den Chip des Schritts tippen) → **Koppeln**.
3. Die App findet das Gerät im lokalen Netz, holt beim Portal ein Token, übergibt es dem Gerät und wartet, bis das Portal bestätigt, dass das Gerät online ist.
4. Nach **Gerät gekoppelt** steht das Gerät in der Liste im Portal und in der App.

Das Log vor der Kopplung:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Danach:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

Der Web-Installer für Firmware übergibt das Token über USB mit dem Befehl `PAIR_TOKEN` (siehe unten).

## Entkopplung

Eine Entkopplung in der App oder im Portal erreicht das Gerät als Befehl `revoke`. Jede Firmware braucht den Handler:

```cpp
// Entkopplung in der App oder im Portal: Geheimnis löschen und auf neue Kopplung warten.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` löscht das Geheimnis, das Netz bleibt: das Gerät wartet wieder auf eine Kopplung. Ohne Handler bleibt das Geheimnis im Gerät, und neu koppeln lässt es sich erst nach `WIPE_IDENTITY`.

## USB-Befehle

Nach dem Beitritt zum Netz nimmt der Kern Zeilen über die serielle Schnittstelle an (115200, jede Zeile endet mit einem Zeilenumbruch):

| Befehl | Antwort | Wirkung |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | Zustand: `state` ist `bound` (gekoppelt) oder `setup` (wartet auf Token), `cloud` ist `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | löscht das Geheimnis wie `revoke`; das Netz bleibt |
| `PAIR_TOKEN:<Token>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | übergibt ein Kopplungstoken; ein gekoppeltes Gerät nimmt kein Token an |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Bevor das Gerät im Netz ist, belegt Improv den Port, und die Befehle antworten nicht. In einem Build mit `IDRYER_DEV_REPL` gehört der Port dem Produkt, und diese Befehle gibt es nicht: siehe [Ausführliche Einrichtung](99-detailed-setup.md).

## Wenn es nicht klappt

- die App findet das Gerät nicht: Telefon und Gerät im selben Netz; Gastnetze blockieren oft die Gerätesuche;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND` oder das Gerät gehört zu einem anderen Konto: im Portal entkoppeln oder `WIPE_IDENTITY` senden und neu koppeln.

## Weiter

[Telemetrie](03-telemetry.md).
