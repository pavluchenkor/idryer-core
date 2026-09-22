---
title: "How to add a new product based on idryer-core"
description: "Checklist for a new iDryer device on the iDryer::Link facade: project, minimal firmware, Wi-Fi and binding in the app, telemetry, settings menu, device card, contract."
---

# How to add a new product based on idryer-core

Use this guide when you are building a new product on top of `idryer-core`: a filament dryer, heating block, lighting module, sensor, or another device. It shows what the core does for you and what the product code has to add.

A complete buildable example is the heated storage cabinet from the Build-Your-Own-iDryer documentation (`example/09-cabinet`): it goes through everything on this page.

---

## What a product is built on

A product talks to the core through one object — the `iDryer::Link` facade (`<iDryer.h>`). Inside `s_link.begin()` and `s_link.loop()` the core:

- takes the Wi-Fi network from the iDryer app over the air (ESPTouch) or from the web installer over USB (Improv) and keeps the connection;
- links the device to an account: waits for a one-time pairing token — from the app over the local network or over the serial port (`PAIR_TOKEN:<token>`) — and exchanges it at the portal for a permanent secret;
- connects to MQTT and publishes `telemetry` and `status` on the `Config` periods;
- announces itself on the local network (mDNS `_idryer._tcp`) and accepts commands from the app over WebSocket;
- publishes the device card manifest.

The lower-level classes (`IdryerRuntime`, `CloudStateMachine`, `LocalAccess` and others) are the core's internals: the pairing token reaches the cloud part only inside `iDryer::Link`. Build a product on the facade.

---

## 1. Project

`idryer-core` goes into `lib/idryer-core/` (a copy or a symlink); PlatformIO takes the core's libraries from its `library.json`. Minimal `platformio.ini`:

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESP8266 transport from espMqttClient's dependencies: does not build on ESP32
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

Without `lib_ignore = ESPAsyncTCP` the build fails in `ESPAsyncTCP.cpp`; without `MQTT_BROKER` and `MQTT_PORT` the core does not compile.

---

## 2. Minimal firmware

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // not an iDryer product: the card comes from the manifest
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // The portal unlinked the device: erase the secret, wait for a new pairing.
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // your sensor functions
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()` starts Wi-Fi, binding, MQTT and local access; `s_link.loop()` has to run all the time, without `delay()`. The `revoke` command comes from the portal when the device is unlinked from the account: `handleRevoke()` erases the secret, and the device waits for a new pairing token.

---

## 3. Wi-Fi and binding — nothing in the code

The firmware contains neither the network password nor account data. The user connects the device in the iDryer app: **Connect a new device** → the **Wi-Fi** step (the app sends the network over ESPTouch) → the **Pairing** step (the app finds the device over mDNS, gets a one-time token from the portal and hands it to the device; the device activates the token at the portal itself). Until Wi-Fi is up, the serial port stays silent: the core keeps it for the web installer (Improv).

Step by step, with the expected log — Build-Your-Own-iDryer, chapter "Starting firmware on the core".

---

## 4. Data: telemetry and status

- The `has*` flags in `Config` define which vocabulary fields go to telemetry and which cells appear on the card.
- Write values to `s_link.telemetry` (`airTempC`, `airHumidityPct`, `heaterTempC`, `heaterPower01`, `fanOn`, `servoOpen`) and `s_link.status` (`mode`, `targetTempC`, `durationS`, `elapsedS`); the core publishes them on the `Config` periods, and `s_link.publishStatusNow()` sends the status at once.
- Your own field — through `s_link.onTelemetryPublish()`, on the card — through `s_link.card().sensor()`; see [Device card](02-add-widget.md).

---

## 5. Settings: the menu

Settings are described in `src/menu/menu.yaml`; the `menu_gen.py` generator produces C++ code, NVS storage and the menu JSON ([Menu as protocol](../08-contracts/02-menu-as-protocol.md)). The product code:

- loads the menu before `s_link.begin()`: `menu.initDefaults()`, `menu.loadFromNVS()`, `menu_sync_state_to_cache()`;
- publishes it with `menu_buildFullJson()` and `s_link.devicePublisher()->publishConfigRaw()` — when the device comes online and on the `get_config` command;
- applies the `set` command with `menu_apply_by_bind()` (value, NVS and cache at once) and publishes the menu again.

The complete code — Build-Your-Own-iDryer, chapter "Menu from YAML".

---

## 6. Device card

What the card shows and which operations it starts is declared with `s_link.card()` — [Device card: the card manifest](02-add-widget.md).

---

## 7. Contract

When you add new topics or change payloads:

1. update `contracts/mqtt_contract.yaml`;
2. run `contracts/regen.sh` and commit the generated files.

---

## Two-chip devices

For an ESP32 that works with a separate controller (for example, RP2040) over UART, the core has the UART bridge `idryer_uart.h`; the working reference is the `idryer-link` firmware.
