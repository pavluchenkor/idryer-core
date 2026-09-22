# Linking to an account

Linking is a one-time procedure: the device gets a one-time pairing token, exchanges it on the portal for a permanent secret and saves the secret to NVS. After that it connects to the portal on its own after every reboot. Until it has a secret, the device is in setup mode and waits for a token.

## In the iDryer app

1. The device is on the network (see [Wi-Fi](01-wifi.md)), the phone is on the same network.
2. **Connect a new device** → the **Pairing** step (if the device is already on the network, tap the step's chip at the top of the window) → **Pair**.
3. The app finds the device on the local network, gets a token from the portal, hands it to the device and waits until the portal confirms that the device is online.
4. After **Device paired**, the device is in the list on the portal and in the app.

The log before pairing:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

After:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

The firmware web installer passes the token over USB with the `PAIR_TOKEN` command (see below).

## Unlinking

Unlinking in the app or on the portal reaches the device as the `revoke` command. Every firmware needs the handler:

```cpp
// Unlinking in the app or on the portal: erase the secret and wait for new pairing.
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` erases the secret and keeps the network: the device waits for pairing again. Without the handler the secret stays in the device, and it can be linked again only after `WIPE_IDENTITY`.

## USB commands

After joining the network, the core accepts lines on the serial port (115200, each line ends with a newline):

| Command | Reply | What it does |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | state: `state` is `bound` (linked) or `setup` (waiting for a token), `cloud` is `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | erases the secret like `revoke`; the network stays |
| `PAIR_TOKEN:<token>` | `PAIR_TOKEN:OK`, `PAIR_TOKEN:ERROR`, `PAIR_TOKEN:ERROR:ALREADY_BOUND` | passes a pairing token; a linked device does not accept a token |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

Before the device joins the network, the port is busy with Improv and the commands do not answer. In a build with `IDRYER_DEV_REPL` the port belongs to the product and these commands do not exist: see [Detailed setup](99-detailed-setup.md).

## If it did not work

- the app does not find the device: the phone and the device are on the same network; guest networks often block device discovery;
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`, or the device belongs to another account: unlink it on the portal or send `WIPE_IDENTITY`, then pair again.

## Next

[Telemetry](03-telemetry.md).
