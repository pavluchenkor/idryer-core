# Schritt 02 — Claim: Bindung an das Portal

Nach diesem Schritt wird Ihr Gerät in Ihrem Konto auf [portal.idryer.org](https://portal.idryer.org/) mit Status Online angezeigt. Alle nachfolgenden Neustarts sind automatisch — kein erneuter Claim erforderlich.

## Was ist Claiming

Claiming ist ein einmaliges Verfahren, bei dem sich der ESP32 bei der idryer.org Cloud registriert und an Ihr Konto bindet. Das Gerät generiert eine 7-stellige PIN mit einer Gültigkeit von 10 Minuten. Sie geben die PIN im Portal ein — die Bindung ist fertig.

Nach dem Claim wird eine `deviceId` im NVS gespeichert — die eindeutige Kennung des Geräts in der Cloud. Bei nachfolgenden Neustarts verbindet sich der ESP32 direkt mit MQTT, ohne den Claim-Fluss zu wiederholen.

## Was Sie benötigen

- ESP32 geflasht von [Schritt 01](01-wifi.md) und mit WiFi verbunden
- Ein Konto auf [portal.idryer.org](https://portal.idryer.org/)
- USB-Kabel und einen offenen Serial Monitor

## Schritte

**1. Überprüfen Sie, dass der Sketch Auto-Claim enthält.** Die folgende Zeile muss in `setup()` sein (sie ist bereits im Beispiel `03_with_improv` vorhanden):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

Dieser Callback wird automatisch ausgelöst, wenn das Gerät das Internet erreicht und erkennt, dass es noch nicht beansprucht wurde.

**2. Öffnen Sie Serial Monitor** und starten Sie das Board neu:

```bash
pio device monitor -b 115200
```

**3. Warten Sie auf die PIN im Protokoll.** Nach WiFi → Provisioning → Warten auf Claim:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

Das Gerät wartet. Die PIN ist 10 Minuten lang gültig.

**4. Gehen Sie zu [portal.idryer.org](https://portal.idryer.org/)** und navigieren Sie zu **Gerät hinzufügen**.

**5. Geben Sie die PIN** aus dem Serial Monitor ein (7 Ziffern, keine Leerzeichen).

**6. Bestätigen Sie die Bindung** im Portal. Der Serial Monitor zeigt dann:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

## Überprüfung

Öffnen Sie die Geräteliste im Portal — das Gerät sollte mit Status **Online** angezeigt werden. Die eingebaute LED blinkt dann einmal pro 500 ms (wenn Sie das Beispiel `01_blink_status` verwenden).

!!! note
    Wenn die PIN abgelaufen ist (mehr als 10 Minuten vergangen) — starten Sie das Board neu. Auto-Claim generiert eine neue PIN.

!!! warning
    Wenn das Gerät bereits von einem anderen Konto beansprucht wurde, geben Sie den Befehl `wipe` im Serial Monitor mit aktiviertem `IDRYER_DEV_REPL=1` ein. Das NVS wird gelöscht, das Board wird neu gestartet und das Claiming beginnt von vorne.

## Nächste Schritte

- [03-telemetry.md](03-telemetry.md) — verbinden Sie einen Sensor und veröffentlichen Sie Messwerte im Portal.
- [02-onboarding.md](02-onboarding.md) — detaillierte Onboarding-Dokumentation für REPL- und Improv-Pfade.
