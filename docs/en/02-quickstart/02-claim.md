# Step 02 — Claim: binding to the portal

After this step your device will appear in your [portal.idryer.org](https://portal.idryer.org/) account with status Online. All subsequent reboots are automatic — no re-claiming needed.

## What is claiming

Claiming is a one-time procedure in which the ESP32 registers with the idryer.org cloud and binds to your account. The device generates a 7-digit PIN valid for 10 minutes. You enter the PIN in the portal — binding is complete.

After claiming, a `deviceId` is saved in NVS — the device's unique identifier in the cloud. On subsequent reboots the ESP32 connects to MQTT directly, without repeating the claim flow.

## What you need

- ESP32 flashed from [Step 01](01-wifi.md) and connected to WiFi
- An account on [portal.idryer.org](https://portal.idryer.org/)
- USB cable and an open Serial Monitor

## Steps

**1. Verify the sketch contains auto-claim.** The following line must be in `setup()` (it is already present in the `03_with_improv` example):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

This callback fires automatically when the device reaches the internet and detects it is not yet claimed.

**2. Open the Serial Monitor** and reboot the board:

```bash
pio device monitor -b 115200
```

**3. Wait for the PIN in the log.** After WiFi → provisioning → awaiting claim:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

The device is waiting. The PIN is valid for 10 minutes.

**4. Go to [portal.idryer.org](https://portal.idryer.org/)** and navigate to **Add device**.

**5. Enter the PIN** from the Serial Monitor (7 digits, no spaces).

**6. Confirm the binding** in the portal. The Serial Monitor will then show:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

## Verification

Open the device list on the portal — the device should appear with status **Online**. The built-in LED will start blinking once every 500 ms (if you are using the `01_blink_status` example).

!!! note
    If the PIN expired (more than 10 minutes passed) — reboot the board. Auto-claim will generate a new PIN.

!!! warning
    If the device is already claimed by another account, enter the `wipe` command in the Serial Monitor with `IDRYER_DEV_REPL=1` enabled. NVS will be erased, the board will reboot, and claiming will start fresh.

## What's next

- [03-telemetry.md](03-telemetry.md) — connect a sensor and publish readings to the portal.
- [02-onboarding.md](02-onboarding.md) — detailed onboarding documentation for REPL and Improv paths.
