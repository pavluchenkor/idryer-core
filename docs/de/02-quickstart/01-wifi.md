# WLAN

Die Firmware enthält kein WLAN-Passwort. Ein Gerät ohne gespeichertes Netz wartet auf Einstellungen; hat es sie bekommen, speichert es das Netz im NVS und verbindet sich danach selbst. Nur Netze mit 2,4 GHz.

## Die iDryer-App (ESPTouch)

Der Hauptweg: die App sendet das Netz per Funk, ohne Kabel.

1. Das Telefon ist in dem Netz, in dem das Gerät arbeiten soll.
2. **Neues Gerät verbinden** → Schritt **WLAN**: Netzwerknamen prüfen, Passwort eingeben, **Gerät verbinden** tippen.
3. Die App sendet die Einstellungen bis zu 90 Sekunden lang und zeigt **Gerät verbunden**.

Bei falschem Passwort wartet das Gerät wieder auf Einstellungen: den Schritt wiederholen.

## Über USB mit Improv

Solange das Gerät kein Netz hat, hört der Kern auf der seriellen Schnittstelle auf das Improv-Protokoll.

1. Board per USB anschließen.
2. [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) in Chrome oder Edge öffnen (Web Serial funktioniert nicht in Safari und Firefox), **Connect** klicken und den Port des Boards wählen.
3. Netzwerknamen und Passwort eingeben.

Der Port ist in dieser Zeit von Improv belegt, deshalb erscheint das Log erst, wenn das Gerät im Netz ist. Den seriellen Monitor schließen, solange Improv arbeitet.

## Im Code, für den Entwicklerstand

```cpp
void setup() {
    // Nur für den Entwicklerstand: das Netz wird im NVS gespeichert, falls es dort noch fehlt.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` schreibt das Netz nur, wenn im NVS noch keines liegt; `setWifiCredentials()` überschreibt immer. Vor `begin()` aufrufen.

!!! warning
    Keine Firmware mit Passwort im Code veröffentlichen: jeder, der sie herunterlädt, bekommt das Passwort.

## Prüfen

Im Log:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## Weiter

[Kopplung mit dem Konto](02-claim.md).
