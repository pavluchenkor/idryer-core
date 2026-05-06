# Onboarding: first device claim

Onboarding is a one-time procedure in which the ESP32 registers with the iDryer cloud and is claimed to your account. Once complete, the device appears in the portal with status Online and state Ready, and all subsequent power-ups are automatic.

## What you will need

- An ESP32 device flashed with a REPL build: env `esp32c3-super-mini-dev` (see [Get started in 5 minutes](01-five-minutes.md)) or any of your dev builds with the flag `IDRYER_DEV_REPL=1`.
- USB cable.
- Account at [portal.idryer.org](https://portal.idryer.org/) (for development — [staging.idryer.org](https://staging.idryer.org/)).

## Path 1. Via Serial REPL (recommended)

The REPL is available only in builds with the flag `IDRYER_DEV_REPL=1`. Open Serial Monitor, enter three commands — the device connects to WiFi, requests a PIN, and is ready to claim.

### 1. Flash the dev build

```bash
pio run -e esp32c3-super-mini-dev -t upload
```

Or use any env where `-DIDRYER_DEV_REPL=1` is set.

### 2. Open Serial Monitor

```bash
pio device monitor -b 115200
```

After boot you will see the prompt:

```
[boot] iDryer dev REPL ready — type 'help'
```

Immediately after, cloud stack messages start appearing in the log:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```

### 3. Connect WiFi

Type in the Serial Monitor console:

```
wifi MyHomeWiFi MySecretPass
```

Response:

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

Credentials are written to NVS. The board immediately calls `WiFi.begin()`. The log will show:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

### 4. Get the PIN and claim in the portal

The device automatically provisions itself and registers a 7-digit PIN. The PIN is valid for 10 minutes.

1. Open [portal.idryer.org](https://portal.idryer.org/) (or staging).
2. Go to **Add device**.
3. Enter the PIN from Serial Monitor.

After a successful claim, the log shows:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

If the PIN expired before you entered it — run the `claim` command to get a new one.

### Useful REPL commands

| Command | What it does | When to use |
|---------|-------------|-------------|
| `help` | Show command list | Remind yourself of syntax |
| `status` | Current state: WiFi, IP, RSSI, online, serial | Connection diagnostics |
| `wifi <ssid> <password>` | Save WiFi credentials to NVS and reconnect | First onboarding or network change |
| `claim` | Manually start the claim flow, get a new PIN | PIN expired or re-claim needed |
| `wipe` | Erase NVS (credentials, claim, menu) and reboot | Factory reset |
| `restart` | Software reboot of the ESP | Quick reboot without physical disconnect |

## Path 2. Via Improv-WiFi (Web Serial)

Improv-WiFi is built into all builds and does not depend on the `IDRYER_DEV_REPL` flag. Suitable for handing off a device to a user or when a terminal is inconvenient. Requires Chrome or Edge — the Web Serial API is not supported in Safari or Firefox.

### 1. Verify the board is flashed

Any prod build will do. Improv-WiFi is always active.

### 2. Open the web page

Go to [https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/), click **Connect**, and select the device's USB port in the browser dialog.

### 3. Enter SSID and password

The page will ask for the network name and password, transmit them to the board via Serial-Improv. The board saves credentials to NVS and connects to WiFi. Provisioning and PIN retrieval happen automatically — the same as in Path 1.

!!! note
    Improv-WiFi cannot run `claim`, `wipe`, or check `status`. Use the REPL for manual claim flow and NVS management.

### When to use each path

| Situation | Recommendation |
|-----------|---------------|
| Embedded developer with a terminal open | REPL |
| Handing the device to a user | Improv-WiFi |
| Need manual `wipe` or repeat `claim` | REPL |
| Safari or Firefox browser | REPL |
| PlatformIO not installed | Improv-WiFi |

## If something went wrong

**PIN not appearing in the log.** Check that the device connected to WiFi: type `status` and verify that the `ip=` field in the response is not empty. Provisioning does not start without WiFi.

**PIN expired.** Enter the `claim` command — the device requests a new registration and prints a fresh PIN.

**Device already claimed to another account.** Enter `wipe` — NVS will be erased, the board will reboot and start onboarding from scratch.

**PIN not accepted by the portal.** Verify you copied all 7 digits with no spaces and that fewer than 10 minutes have passed since the PIN appeared.

**Improv-WiFi does not see the device in the browser.** Make sure you are using Chrome or Edge and that the ESP32 USB driver is installed.

## What to do next

- Full Link API: [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- Add a sensor or peripheral: [../04-patterns/](../04-patterns/)
