# Detailed setup

If this is your first time here — go to [Get started in 5 minutes](01-five-minutes.md); this page covers advanced setup and troubleshooting.

Short path: wire up the library, flash an example, see the blinking LED and the device in the portal.

## What to prepare

- ESP32 board (recommended: ESP32-C3 DevKit, Super Mini, XIAO ESP32-S3, Waveshare ESP32-S3 Zero).
- PlatformIO with framework `arduino`, platform `espressif32`.
- WiFi 2.4 GHz with internet access.
- Account at [portal.idryer.org](https://portal.idryer.org/) for claiming.

## Step 1. Wire up the library

In your product's `platformio.ini`:

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; only needed for mqtt_with_local_ws

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

## Step 2. Create `secrets.h`

Copy [`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) to `include/secrets.h` in your project and fill in your SSID/password. The file must be in `.gitignore`.

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

`IDRYER_API_BASE` is normally set via `build_flags`, not through secrets.h.

## Step 3. Open the first example

The simplest one is [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino). Copy it as your starting point:

- Requires no sensors, peripherals, or LAN WS.
- Requires no manual `handleCommand` — the built-in fallback in `IdryerRuntime` handles basic commands.
- LED blinks when the device is online — that is the success indicator.

## Step 4. Flash and observe

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

Expected log sequence:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← waiting for claim
[CSM] PIN: 1234567   expires in 600s          ← if auto-claim is enabled
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← ready, LED starts blinking
[RT]  Cloud Online
```

## Step 5. Claim the device to your account

Auto-claim is already enabled in the example. The PIN appears in the log. Enter it at [portal.idryer.org](https://portal.idryer.org/) → "Add device". After claiming, `CloudStateMachine` transitions to `Online`.

## What to do next

The following examples each introduce one new level of complexity:

| Example | What is added |
|---------|--------------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | custom `handleCommand`, handling `commands/invoke` and `commands/set` |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | WiFi provisioning via Improv (no hardcoded credentials) |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | local LAN WebSocket server + `DevicePublisher` (one publish — two transports) |

## Dev REPL via Serial (no portal, no browser)

An alternative path for developers — see the full claim flow directly in a standard Serial monitor, without Improv and without the portal UI.

In `platformio.ini`, create a dev environment with the flag `-DIDRYER_DEV_REPL=1`:

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

What the flag enables:
- HAL logs to `Serial` start **immediately** from boot (no silence until WiFi connects).
- Improv provisioning is **disabled** — Serial is free for interactive commands.
- A simple REPL appears in `main.cpp`: `wifi`, `claim`, `status`, `wipe`, `restart`, `help`.

Full flow:

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

In the monitor:

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — enter in portal
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

The REPL accepts commands regardless of the line-ending setting in the Serial monitor (`\n`, `\r`, or idle timeout 120 ms) — works in any terminal, including `pio device monitor`, Arduino IDE Serial Monitor, `screen`, `picocom`.

The production build (`-e my-device-prod`, without `IDRYER_DEV_REPL`) uses Improv via Chrome (`https://www.improv-wifi.com/`) and contains no REPL code — the flag is compile-time, saving Flash.

`secrets.h` with `WIFI_SSID/WIFI_PASSWORD` (Step 2) remains a separate path for headless CI/auto-flash scenarios — works in both environments.

After any of the examples are up and running, read:

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — object order in `main.cpp`.
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — how data moves.
- [04-patterns/](../04-patterns/) — recipes: add sensor, peripheral, transport.
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — full checklist for a new product.
- [10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — what to do if the stack is stuck.
