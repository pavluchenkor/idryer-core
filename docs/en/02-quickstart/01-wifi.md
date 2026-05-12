# Step 01 — WiFi provisioning with Improv

After this step your ESP32 will be connected to WiFi and the credentials will be saved to NVS for automatic reconnection on the next reboot. Portal and MQTT come in the next step.

## What you need

**Hardware:**

- ESP32-C3 board (DevKit, Super Mini, or compatible)
- USB cable (USB-C or Micro-USB depending on your board)

**Software:**

- PlatformIO in VS Code
- Chrome or Edge browser (Web Serial API is not supported in Safari or Firefox)

## Steps

**1. Create `platformio.ini`** in the root of your project:

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Replace `board` with the value for your board (`esp32-c3-devkitm-1`, `seeed_xiao_esp32c3`, etc.).

**2. Copy the example.** Take the contents of [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) and save it as `src/main.cpp` in your project.

**3. Set the ChipFamily.** In the copied file, find the line:

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

Make sure the ChipFamily matches your chip: `CF_ESP32_C3`, `CF_ESP32_S3`, or `CF_ESP32`.

**4. Flash:**

```bash
pio run -e improv-demo -t upload
```

**5. Open [improv-wifi.com/serial](https://www.improv-wifi.com/serial/)** in Chrome or Edge. Click **Connect** and select the device USB port from the browser dialog.

**6. Enter the SSID and password** for your 2.4 GHz network. The web page will send the credentials to the board over Serial-Improv. The board will save them to NVS.

## Verification

Open the Serial Monitor:

```bash
pio device monitor -b 115200
```

After a successful connection you will see:

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

If this line does not appear, see the troubleshooting link below.

!!! note
    If credentials are already saved in NVS from a previous run, the board connects to WiFi at boot automatically — Improv is not needed.

## What's next

- [02-claim.md](02-claim.md) — bind the device to your idryer.org account.
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — if WiFi does not connect.
