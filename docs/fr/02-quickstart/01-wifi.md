# Wi-Fi

Le firmware ne contient pas de mot de passe du réseau. Un appareil sans réseau enregistré attend les réglages ; une fois reçus, il enregistre le réseau en NVS et s'y connecte ensuite tout seul. Réseaux 2,4 GHz uniquement.

## L'application iDryer (ESPTouch)

La voie principale : l'application envoie le réseau par les ondes, sans fil.

1. Le téléphone est sur le réseau où fonctionnera l'appareil.
2. **Connecter un nouvel appareil** → étape **Wi-Fi** : vérifiez le nom du réseau, saisissez le mot de passe, touchez **Connecter l’appareil**.
3. L'application envoie les réglages pendant 90 secondes au maximum et affiche **Appareil connecté**.

Avec un mauvais mot de passe, l'appareil attend de nouveau les réglages : recommencez l'étape.

## Par USB avec Improv

Tant que l'appareil n'a pas de réseau, le noyau écoute le protocole Improv sur le port série.

1. Branchez la carte en USB.
2. Ouvrez [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) dans Chrome ou Edge (Web Serial ne fonctionne pas dans Safari et Firefox), cliquez sur **Connect** et choisissez le port de la carte.
3. Saisissez le nom du réseau et le mot de passe.

Le port est alors occupé par Improv, le journal n'apparaît donc qu'une fois l'appareil sur le réseau. Fermez le moniteur série pendant qu'Improv travaille.

## Dans le code, pour un banc de développement

```cpp
void setup() {
    // Banc de développement uniquement : le réseau est enregistré en NVS s'il n'y est pas encore.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` n'écrit le réseau que si la NVS n'en contient pas encore ; `setWifiCredentials()` réécrit toujours. Appelez-les avant `begin()`.

!!! warning
    Ne publiez pas de firmware avec un mot de passe dans le code : quiconque le télécharge obtient le mot de passe.

## Vérification

Dans le journal :

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## Suite

[Association au compte](02-claim.md).
