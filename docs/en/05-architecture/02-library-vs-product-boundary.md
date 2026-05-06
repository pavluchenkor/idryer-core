# Boundary: library and product

## What lives in the library

The library (`lib/idryer-core/`) contains:

- The entire network stack: WiFi, HTTP, MQTT, TLS.
- The provisioning/claiming protocol.
- The cloud state machine (`CloudStateMachine`).
- UART bridge and frame protocol.
- Integration clients (Bambu, HA, Moonraker).
- Device interfaces (`IWifiManager`, `ICredentialStore`, `IHttpClient`, `IProfile`).
- Arduino implementations of those interfaces.
- MQTT topics and publish/subscribe logic.

The test for code belonging in the library: **any product with any hardware can use it without modification**.

## What lives in the product

The product (`src/`) contains:

- `IProfile` implementation — configuration, info payload, `applyConfig`.
- Business logic specific to the device (LED control, drying, heating).
- `onInvoke` / `onSetCommand` handlers.
- Product sensors and telemetry publishing.
- Peripheral initialization (FastLED, Wire, ImprovWiFi).
- Composition root in `main.cpp`.

The test for code belonging in the product: **without changing the hardware or configuration, it is meaningless**.

## Concrete examples

| Code | Where it lives | Why |
|------|---------------|-----|
| `MqttClient` | library | every product needs MQTT |
| `CloudStateMachine` | library | provisioning/claiming is the same for all |
| `ArduinoWifiManager` | library | WiFi connection does not depend on the product |
| `LedStripProfile` | product | specific to Storage Link TODO: use consistent Storage name throughout the doc |
| `LedStripExecutor` | product | controls FastLED, not needed by other devices |
| `Sht31ClimateSensor` | product | a specific sensor for a specific product |
| `StorageTelemetryPublisher` | product | knows the Storage Link telemetry format |
| `IProfile` | library | contract that the library calls |
| `BambuClient` | library | integration is reused across iDryer and iHeater |

## Interfaces as the boundary

The library knows about the product only through `IProfile`. All interaction goes through five methods:

```cpp
profile->onOnline();               // library → product: first time going online
profile->loop();                   // library → product: every cycle
profile->buildInfoJson(buf, len);  // library → product: info payload needed
profile->getConfig(doc);           // library → product: config needed
profile->applyConfig(id, val);     // library → product: set command received
```

The product knows about the library through `MqttClient` (for publishing telemetry/events) and through `ActionDispatcher` callbacks (for commands).

## What must not cross the boundary

- The library must not include product headers.
- The product must not call `CloudStateMachine::handleProvisioning()` or other private stack methods directly — only through the public API.
- Product telemetry is published directly via `s_mqtt.publishTelemetry()` — the runtime does not see it.
