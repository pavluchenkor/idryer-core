# Wi-Fi connection and setup mode

The device receives the network password in two ways: over the wire from the web installer (Improv protocol) and over the air from the **iDryer** mobile app for Android and iOS (ESPTouch v2 protocol). After that the password lives in NVS and survives reboots.

`EspTouchProvisioner` (`platform/arduino/`) handles this. The product does not call anything — `Link` starts and drives it; the callbacks exist only to show setup progress on a screen.

The protocols are not compatible across versions: the app must send ESPTouch **v2**. In the first version the data was carried by frame lengths, and those broke whenever a router relayed the broadcast between the 2.4 and 5 GHz bands — a single mesh network with one shared name made it work only every other time.

Understanding this behaviour matters for one reason: the device must restore the connection silently when it can, and call the owner only when it cannot. It is easy to fail in both directions — either bother the owner every time the router hiccups, or sit offline forever with a wrong password.

## Four phases

The provisioner is alive **only until the first successful connection**. `Link::loop()` calls it while Wi-Fi is down; after the first `WL_CONNECTED` the cloud state machine takes over and setup mode never starts again — until the next reboot.

### Phase 1. Boot, password is in memory

Persistent mode, no diagnostics.

Every `kBootRetryMs` (6 seconds) — a new connection attempt. This continues for `kConnectFallbackMs` (90 seconds). Connected — we work; not connected — setup mode starts.

Why no diagnostics: the password is not new, it worked at some point. There is no need to find out whether the signal is weak or the password was changed — if there is no link within the window, we call the owner and they will sort it out on the spot.

Why the retries are needed: `WiFi.begin()` is called once, from `Link::begin()`. The cloud state machine does not run before `WL_CONNECTED`, and the Arduino auto-reconnect does not cover `reason` 202 and 205. Without retries there is exactly one attempt, and on a weak signal the device enters setup mode with a perfectly valid password in memory.

Why 90 seconds — counted from the worst case, when the power comes back and everything boots at once:

- **up to 60 seconds** — the router boot. Broadband Forum industry requirement [TR-124 Issue 9](https://rg-device-requirements.broadband-forum.org/), clause `GEN.OPS.9`: "The RG MUST complete power up in 60 seconds or less". A caveat: the requirement covers the full gateway power-up, there is no separate limit for the SSID appearing. Measurements on mass-market hardware give 34–49 seconds until the network is on the air, so 60 is a sensible upper bound rather than an average;
- **plus our own connection** — usually 2–5 seconds, but at an `RSSI` around −85 dBm it stretches to twenty.

Hence 90: sixty for the router and thirty for ourselves. Less than that risks calling the owner right before the network shows up; noticeably more leaves the owner staring at a device with no link and no explanation.

A router and an access point powered on together boot in parallel, not one after another — their times must not be added up. If you use the stricter criterion "the client got an address and reached the internet", the count goes to 90–120 seconds: that also includes WAN, DHCP or PPPoE and modem synchronisation. We do not need to wait for that — the device only has to join the local network.

### Phase 2. Boot, no password in memory

Setup mode starts immediately — there is nothing to wait for.

### Phase 3. Password has just been received

Smart mode, `driveConnect()`.

Here the password is **fresh and may be wrong** — the owner could have mistyped it. So the provisioner tells the failure reasons apart:

- counts consecutive authentication failures (`kAuthFailLimit`, 8);
- after the fifth one it scans the air and remembers the best `RSSI`;
- if the `RSSI` is worse than `kWeakRssi` (−80 dBm) the reset is cancelled: the same `reason` 15 and 202 also come from lost auth frames with a correct password, and connecting may take minutes. There the deadline is different — `kWeakSignalGiveUpMs` (10 minutes);
- with a confident signal and eight failures it returns to setup mode: otherwise a typo has no way out short of reflashing.

Plus a watchdog for complete silence (`kWatchdogMs`, 20 seconds): when there are neither events nor a connection, the stack is kicked with `esp_wifi_disconnect()`.

### Phase 4. The link is lost while running

The provisioner is no longer called. The cloud state machine restores the link silently, setup mode does not start — the owner is not disturbed. From the device's point of view "the router was switched off for the night" and "the router was replaced" are indistinguishable, and it does not try to guess.

## How to tell the device the network has changed

Reboot it.

After the reboot phase 1 begins: the old password does not fit, 90 seconds pass in vain, setup mode starts — and a new password can be sent from the phone or over the wire. No heuristics, just power off and on.

## A property: the network is not picked up again from setup mode

While the device is listening to the air it does **not try to connect** to the saved network: the radio is busy receiving, and connection attempts stop until a new password arrives.

The practical consequence: if the router took longer than 90 seconds to boot and the device had already entered setup mode, it will not return to the network on its own even after the network appears. Either send the password or reboot the device.

This is a property of the mode, not an oversight: listening to the air and establishing a connection at the same time is not possible.

## Timeouts

| Constant | Value | What it sets |
|---|---|---|
| `kBootRetryMs` | 6 s | retry period in phase 1 |
| `kConnectFallbackMs` | 90 s | how long we try before calling the owner |
| `kReconnectDelayMs` | 700 ms | pause after a disconnect before a new attempt (phase 3) |
| `kWatchdogMs` | 20 s | stack silence after which it gets kicked |
| `kAuthFailLimit` | 8 | consecutive auth failures before returning to setup |
| `kWeakRssi` | −80 dBm | the "weak signal" threshold |
| `kWeakSignalGiveUpMs` | 10 min | how long we tolerate a weak signal before giving up |
| `kRestartMs` | 10 min | setup mode restart if no password ever arrives |

## What the owner sees

Entering setup mode raises the `Notice` callback:

- `Listening` — ordinary setup: either there is no network in memory, or the link has already worked before;
- `CheckPassword` — the network is configured but has never let the device in. The only remaining explanation is a wrong password.

The product decides how to present this. In `idryer-touch` it is a full-screen notice.
