# Association au compte

L'association est une opération unique : l'appareil reçoit un jeton d'association à usage unique, l'échange sur le portail contre un secret permanent et enregistre le secret en NVS. Ensuite il se connecte tout seul au portail après chaque redémarrage. Tant qu'il n'a pas de secret, l'appareil est en mode configuration et attend un jeton.

## Dans l'application iDryer

1. L'appareil est sur le réseau (voir [Wi-Fi](01-wifi.md)), le téléphone sur le même réseau.
2. **Connecter un nouvel appareil** → étape **Association** (si l'appareil est déjà sur le réseau, touchez la puce de l'étape en haut de la fenêtre) → **Associer**.
3. L'application trouve l'appareil sur le réseau local, obtient un jeton du portail, le remet à l'appareil et attend que le portail confirme que l'appareil est en ligne.
4. Après **Appareil associé**, l'appareil figure dans la liste du portail et de l'application.

Le journal avant l'association :

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

Après :

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

L'installateur web de firmware transmet le jeton par USB avec la commande `PAIR_TOKEN` (voir plus bas).

## Dissociation

Une dissociation dans l'application ou sur le portail arrive à l'appareil sous la forme de la commande `revoke`. Chaque firmware a besoin du gestionnaire :

```cpp
// Dissociation dans l'application ou sur le portail : effacer le secret et attendre une nouvelle association.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` efface le secret et garde le réseau : l'appareil attend de nouveau une association. Sans gestionnaire, le secret reste dans l'appareil, et il ne pourra être réassocié qu'après `WIPE_IDENTITY`.

## Commandes USB

Une fois sur le réseau, le noyau accepte des lignes sur le port série (115200, chaque ligne se termine par un retour à la ligne) :

| Commande | Réponse | Effet |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | état : `state` vaut `bound` (associé) ou `setup` (attend un jeton), `cloud` vaut `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | efface le secret comme `revoke` ; le réseau reste |
| `PAIR_TOKEN:<jeton>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | transmet un jeton d'association ; un appareil associé ne l'accepte pas |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Avant l'arrivée sur le réseau, le port est occupé par Improv et les commandes ne répondent pas. Dans une compilation avec `IDRYER_DEV_REPL`, le port appartient au produit et ces commandes n'existent pas : voir [Configuration détaillée](99-detailed-setup.md).

## Si cela n'a pas marché

- l'application ne trouve pas l'appareil : téléphone et appareil sur le même réseau ; les réseaux invités bloquent souvent la découverte des appareils ;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`, ou l'appareil appartient à un autre compte : dissociez-le sur le portail ou envoyez `WIPE_IDENTITY`, puis associez-le de nouveau.

## Suite

[Télémétrie](03-telemetry.md).
