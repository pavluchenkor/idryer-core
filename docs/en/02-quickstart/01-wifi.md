# Wi-Fi

The firmware has no network password. A device without a saved network waits for settings; once it gets them, it saves the network to NVS and connects on its own from then on. 2.4 GHz networks only.

## The iDryer app (ESPTouch)

The main way: the app sends the network over the air, no wires.

1. The phone is on the network the device will use.
2. **Connect a new device** → the **Wi-Fi** step: check the network name, enter the password, tap **Connect device**.
3. The app sends the settings for up to 90 seconds and shows **Device connected**.

With a wrong password the device waits for settings again: repeat the step.

## Over USB with Improv

While the device has no network, the core listens for the Improv protocol on the serial port.

1. Connect the board over USB.
2. Open [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) in Chrome or Edge (Web Serial does not work in Safari and Firefox), click **Connect** and choose the board's port.
3. Enter the network name and password.

The port is busy with Improv at this time, so the log appears only after the device joins the network. Close the serial monitor while Improv is working.

## In code, for a developer bench

```cpp
void setup() {
    // Developer bench only: the network is saved to NVS if it is not there yet.
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` writes the network only if NVS does not have one yet; `setWifiCredentials()` always overwrites. Call them before `begin()`.

!!! warning
    Do not release firmware with a password in the code: everyone who downloads it gets the password.

## Check

In the log:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## Next

[Linking to an account](02-claim.md).
