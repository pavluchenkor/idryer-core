# Wi-Fi

Ve firmwaru není heslo k síti. Zařízení bez uložené sítě čeká na nastavení; jakmile ho dostane, uloží síť do NVS a dál se připojuje samo. Jen sítě 2,4 GHz.

## Aplikace iDryer (ESPTouch)

Hlavní cesta: aplikace pošle síť vzduchem, bez drátů.

1. Telefon je v síti, ve které bude zařízení pracovat.
2. **Připojit nové zařízení** → krok **Wi-Fi**: zkontrolujte název sítě, zadejte heslo, klepněte na **Připojit zařízení**.
3. Aplikace posílá nastavení až 90 sekund a ukáže **Zařízení připojeno**.

Při špatném hesle zařízení znovu čeká na nastavení: zopakujte krok.

## Přes USB pomocí Improv

Dokud zařízení nemá síť, jádro poslouchá na sériovém portu protokol Improv.

1. Připojte desku přes USB.
2. Otevřete [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) v Chrome nebo Edge (Web Serial v Safari a Firefoxu nefunguje), klikněte na **Connect** a vyberte port desky.
3. Zadejte název sítě a heslo.

Port je v tu chvíli obsazen Improvem, proto se log objeví až po připojení zařízení k síti. Sériový monitor během práce Improvu zavřete.

## V kódu, pro vývojářský stand

```cpp
void setup() {
    // Jen pro vývojářský stand: síť se uloží do NVS, pokud tam ještě není.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` zapíše síť, jen když v NVS ještě žádná není; `setWifiCredentials()` přepisuje vždy. Volejte je před `begin()`.

!!! warning
    Nevydávejte firmware s heslem v kódu: dostane ho každý, kdo si ho stáhne.

## Kontrola

V logu:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## Dál

[Spárování s účtem](02-claim.md).
