# Publishing via devicePublisher

## When to use

`iDryer::Link` already contains two built-in transports: MQTT (cloud) and Local WebSocket (LAN). An additional transport is not needed for most tasks.

Use `s_link.devicePublisher()` when the product assembles its own payload and must send it to both channels simultaneously — for example, when publishing the menu configuration in response to `commands/get_config`.

## Ready-to-use code

```cpp
// main.cpp (fragment)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// Publish an arbitrary JSON payload to MQTT and Local WS in a single call.
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // product function
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

A single `publishConfigRaw` call delivers the payload to the MQTT topic `idryer/{serial}/config` and to all active LAN WS clients. No additional clients or topics need to be created.

## Explanation

`devicePublisher()` is the facade's dual-publish helper. Use it instead of calling `mqttClient()` or `LocalAccess` directly, unless you need to publish to a non-standard topic.

Telemetry and status are published automatically by the facade on a timer — `devicePublisher()` is not needed for those.

## When a third transport is needed

Adding a third channel (BLE, Serial JSON, UART proxy) is an architectural extension of the facade, not a recipe pattern. The overwhelming majority of devices do not require this.

If you do need it — entry points are in `lib/idryer-core/src/cloud/` (cloud state machine, MQTT) and `lib/idryer-core/src/` (local access). Before proceeding, confirm that the built-in MQTT and Local WS are insufficient for your use case.

## Full example in the repo

`publishFullMenu()` in `iDryer-Storage/src/main.cpp:171` — publishing the full JSON menu via `s_link.devicePublisher()->publishConfigRaw(buf, len)`.
