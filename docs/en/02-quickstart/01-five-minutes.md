# Get started in 5 minutes

After this page your ESP32 will be flashed, will connect to WiFi, and will appear in [portal.idryer.org](https://portal.idryer.org/) with status Online. Requirements: ESP32-C3 (DevKit, Super Mini, or compatible), USB cable, PlatformIO in VS Code.

## 1. Prepare secrets.h

Copy [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) to `include/secrets.h` in your project and set your WiFi SSID and password (2.4 GHz only):

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

Add `include/secrets.h` to `.gitignore`.

## 2. Configure platformio.ini

Create `platformio.ini` in the project root:

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://path/to/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

Change `board` to match your board. Replace `path/to/idryer-core` with the actual path to the library.

## 3. Copy the 01_blink_status example

Copy the contents of [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) into `src/main.cpp` of your project. The example requires no sensors or additional dependencies — only a minimal composition root.

## 4. Flash

```bash
pio run -e blink-demo -t upload
```

## 5. Open Serial Monitor

```bash
pio device monitor -b 115200
```

Expected log sequence:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

After entering the PIN in the portal (step 6):

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

If the device stopped at the `PIN: ...` message — that is expected; proceed to step 6.

## 6. Claim the device in the portal

Open [portal.idryer.org](https://portal.idryer.org/), go to **Add device**, and enter the PIN from Serial Monitor. After a successful claim the device will transition to `Online` and the built-in LED will blink every 500 ms.

Detailed claim flow: [Onboarding](02-onboarding.md).

## What to do next

- Add a sensor — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- Add a peripheral — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- Full API reference — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- How it works internally — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
