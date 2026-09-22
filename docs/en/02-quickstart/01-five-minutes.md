---
title: "Run a device on idryer-core in 5 minutes"
description: "Your first device on idryer-core: a PlatformIO project, minimal firmware, Wi-Fi and linking to your account in the iDryer app."
---

# Run a device on idryer-core in 5 minutes

After this page the ESP32 is on the network, linked to your account and visible on the [portal.idryer.org](https://portal.idryer.org/) portal and in the iDryer app. You need: an ESP32-C3 board (DevKit, Super Mini or compatible), a USB cable, PlatformIO in VS Code, a phone with the iDryer app, a 2.4 GHz Wi-Fi network.

## 1. PlatformIO project

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← a copy, a git submodule or a symbolic link
└── src/
    └── main.cpp
```

`platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP is the ESP8266 transport from espMqttClient dependencies: it does not build on ESP32.
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; Board without USB-UART (ESP32-C3 SuperMini): Serial over USB.
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

The core libraries (MQTT, ArduinoJson, WebSockets, Improv) come on their own from the core's `library.json`. A board with USB-UART does not need the last two flags; set `board` for your board.

## 2. Code

Copy the [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) example into `src/main.cpp`:

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // your own device
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// Blink period by state; 0 means do not blink.
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // Unlinking: the core erases the secret and waits for pairing again.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // The core publishes telemetry fields itself, every 30 s (every 60 s when idle).
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

The firmware has neither a network password nor account data: the device gets the network and the pairing from the app.

## 3. Flash and open the log

```bash
pio run -t upload
pio device monitor -b 115200
```

While the device has no Wi-Fi, the log is silent: the core keeps the port for Improv. The LED blinks fast: the device is waiting for a network and pairing.

## 4. Wi-Fi and pairing in the app

1. Connect the phone to the Wi-Fi network the device will use and sign in to the iDryer app with your portal account.
2. On the home screen, tap **Connect a new device**: the **Wi-Fi** step opens.
3. Check the network name, enter the password and tap **Connect device**. The app sends the settings for up to 90 seconds; when the device joins, **Device connected** appears. Tap **Next**.
4. On the **Pairing** step, tap **Pair**. The app finds the device on the network, gets a one-time pairing token from the portal and hands it to the device.
5. After **Device paired**, the device appears in the device list on the portal and in the app.

The log after joining the network:

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

After pairing:

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. Check

- the device is online on the portal and in the app;
- the card shows the ESP32 chip temperature;
- the LED blinks once a second. Steady on or off means no connection to the portal.

## If it did not work

- the app did not see the device connect: check the password and that the network is 2.4 GHz; with a wrong password the device waits for settings again. Other ways to pass the network: [Wi-Fi](01-wifi.md);
- the **Pairing** step did not find the device: the phone and the device must be on the same network; guest networks often block device discovery. More: [Linking to an account](02-claim.md);
- build and flags: [Detailed setup](99-detailed-setup.md).

## Next

- [Telemetry](03-telemetry.md): a sensor and your own value on the card.
- [Core examples](https://github.com/pavluchenkor/idryer-core/tree/main/examples): card actions, your own sensors and controls.
