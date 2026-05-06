# Public API: iDryer::Link

`iDryer::Link` is the single entry point for the embedded developer. The facade hides the entire SDK stack: WiFi/Improv, cloud state machine, HTTP claim, MQTT, local WebSocket, NVS. The product only needs to fill `telemetry`/`status` fields, register callbacks, and call `begin()`/`loop()`.

---

## Lifecycle

Typical `main.cpp` skeleton:

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // only needed if setCommandHandler is used

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — strictly AFTER begin(): begin() installs its own dispatcher
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## Configuration: `iDryer::Config`

Filled once in `main.cpp`, passed to the `Link` constructor. All fields use aggregate init (C++ designated initializers).

| Field | Type | Purpose | Note |
|-------|------|---------|------|
| `deviceType` | `DeviceType` | Device type | **required** |
| `unitsCount` | `uint8_t` | Number of units (chambers), 1..`MAX_UNITS` (4) | **required** |
| `hasAirTemp` | `bool` | Air temperature sensor present | false = field omitted from JSON |
| `hasAirHumidity` | `bool` | Humidity sensor present | false = field omitted from JSON |
| `hasHeaterTemp` | `bool` | Heater temperature sensor present | — |
| `hasHeaterPower` | `bool` | Heater power sensor present | — |
| `hasFanStatus` | `bool` | Fan status present | — |
| `hasScales` | `bool` | Scales present | — |
| `hasRfid` | `bool` | RFID reader present | — |
| `allowHa` | `bool` | Allow Home Assistant integration | false = SDK does not create a client |
| `allowBambu` | `bool` | Allow Bambu Lab LAN integration | — |
| `allowMoonraker` | `bool` | Allow Moonraker/Klipper integration | — |
| `telemetryPeriodMs` | `uint32_t` | Auto-publish period for `Telemetry` (ms) | 0 = do not publish |
| `statusPeriodMs` | `uint32_t` | Auto-publish period for `Status` (ms) | 0 = do not publish |
| `hardwareVersion` | `const char*` | Hardware version string | **required** |
| `firmwareVersion` | `const char*` | Firmware version string | **required** |

---

## Class `iDryer::Link`

### Constructor

```cpp
explicit Link(const Config& cfg);
```

Takes the configuration by const reference. `CFG` must exist for the full object lifetime (typically `static const`).

### Methods

#### `begin()`

```cpp
bool begin();
```

Brings up the entire SDK stack: WiFi/Improv, cloud state machine, HTTP claim, MQTT, local WebSocket, NVS persistence.

Call once in `setup()`. Returns `true` on successful initialization.

```cpp
void setup() {
    link.begin();
}
```

#### `loop()`

```cpp
void loop();
```

The only required tick. Services WiFi/MQTT/LocalAccess, and auto-publishes telemetry and status on their timers.

Call every iteration of `loop()`. Without this call the connection is not maintained.

```cpp
void loop() {
    link.loop();  // first in loop(), before product logic
}
```

*Source: `iDryer-Storage/src/main.cpp:253`, `iHeater-link/src/main.cpp:381`.*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

Immediately publishes the current state of `link.telemetry`, regardless of the `telemetryPeriodMs` timer.

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

Immediately publishes the current state of `link.status`. Use after processing a command when the new state must be reflected in the portal right away.

```cpp
// iHeater-link/src/main.cpp:238
device().publishStatusNow();
```

#### `raiseEvent()`

```cpp
void raiseEvent(EventKind   severity,
                const char* event,
                const char* message,
                uint8_t     unitId = 0xFF);
```

Publishes an event to the topic `idryer/{serial}/events`. Sent immediately.

| Parameter | Type | Purpose |
|-----------|------|---------|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | Event code, e.g. `"OVERHEAT"`, `"SESSION_COMPLETE"` |
| `message` | `const char*` | Arbitrary debug text |
| `unitId` | `uint8_t` | Unit index (0..unitsCount-1) or `0xFF` for device-wide |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

Registers a callback for business commands (`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`) arriving over MQTT or Local WS. The command source is transparent.

`RequestCallback` = `std::function<void(const iDryer::Request&)>`

```cpp
link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start: myStart(r.unitId, r.targetTempC); break;
        case iDryer::RequestKind::Stop:  myStop(r.unitId);                 break;
        default: break;
    }
});
```

**Important:** if `runtime()->setCommandHandler(...)` is set, this callback is not called — the full dispatcher intercepts all commands.

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

Registers a callback for `commands/profile` — a multi-step drying schedule.

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

Called when an integration connection state changes (HA, Bambu, Moonraker). Optional callback.

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

Called when the cloud claim flow returns a PIN for entry in the portal.

`ClaimPinCallback` = `std::function<void(const char* pin, uint32_t expiresInSeconds)>`

```cpp
// iHeater-link/src/main.cpp:367
device().onClaimPin([](const char* pin, uint32_t expiresInSeconds) {
    Serial.printf("CLAIM_PIN:%s:%u\n", pin, expiresInSeconds);
});
```

#### `isOnline()`

```cpp
bool isOnline() const;
```

Returns `true` if the device is registered and the MQTT session is active.

```cpp
// iHeater-link/src/main.cpp:281
if (device().isOnline()) { ... }
```

#### `serial()`

```cpp
const char* serial() const;
```

Device serial number (string from NVS, assigned during claim). Empty string before claim completes.

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

Writes WiFi credentials to NVS only if they are not yet set. Call before `begin()`. Used in dev environments with hardcoded credentials.

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

Always overwrites WiFi credentials in NVS. Dev helper and forced re-provisioning.

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

Manually starts the cloud claim flow (provision → register → check-claim). On success calls the registered `onClaimPin` callback. Returns `true` if the request was accepted.

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

Removes the device token from NVS and reboots the chip. After reboot the device is unclaimed — the auto-claim flow starts again. This function does not return.

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

Outlet to the integrations manager — for product-side wiring (Moonraker chamber target callbacks, Bambu printer status, etc.).

Requires `#include <integrations/common/link_integrations_manager.h>`.

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

Outlet to the SDK MQTT client — for components that publish their own topics or integrate into command routing (e.g., `MenuBridge`).

Requires `#include <mqtt/mqtt_client.h>`.

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

Outlet to the dual-publish helper — sends one payload to both MQTT and Local WS simultaneously. Use for product responses that must reach the LAN client the same way auto-published telemetry does.

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

Outlet to the SDK runtime — used to set a full command handler instead of the facade dispatcher. After `setCommandHandler(...)` the facade's `onRequest`/`onProfile` are no longer called via the MQTT path.

**Important:** call strictly after `begin()` — `begin()` installs its own dispatcher, which must be overwritten.

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// Handler signature:
// void handleCommand(const char* cmd, JsonObjectConst data);
```

Requires `#include <runtime/idryer_runtime.h>`.

---

### Telemetry fields {#telemetry-fields}

Filled by the product in `loop()`. The SDK reads them on the `telemetryPeriodMs` timer and publishes to MQTT and Local WS.

| Field | Type | Config flag | Purpose |
|-------|------|-------------|---------|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | Air temperature, °C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | Humidity, % |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | Heater temperature, °C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | Heater power, 0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | Fan status |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | Weight, grams |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

`unitId` = 0 for the first (or only) unit. The index must be < `Config.unitsCount`.

`Status` fields — same structure, but for operational state:

| Field | Type | Purpose |
|-------|------|---------|
| `status.mode[unitId]` | `UnitMode` | Current unit mode |
| `status.targetTempC[unitId]` | `float` | Target temperature |
| `status.durationS[unitId]` | `uint32_t` | Requested duration, s (0 = indefinite) |
| `status.elapsedS[unitId]` | `uint32_t` | Time elapsed since session start, s |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### Callback registration via runtime

If full control over incoming commands is needed (e.g., the product handles `get_config`, `set`, non-standard `invoke`):

```cpp
// Signature — from idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// Registration — strictly after link.begin()
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — command string (`"set"`, `"invoke"`, `"ping"`, `"get_config"`).
`data` — ArduinoJson `JsonObjectConst` with payload.

With this approach, `onRequest()` and `onProfile()` are not called from the MQTT path — the product handles commands directly.

---

## Enumerations

### `iDryer::DeviceType`

| Value | Numeric | Purpose |
|-------|---------|---------|
| `Unknown` | 0 | None / undefined |
| `Dryer` | 1 | Dryer (iDryer LINK) |
| `Heater` | 2 | Heater |
| `StorageLink` | 4 | Storage Link (ESP32-C3 + LED) |
| `IHeaterLink` | 5 | iHeater Link |

### `iDryer::UnitMode`

`Idle`, `Drying`, `Storage`, `Profile`, `Fault`, `Unknown`

### `iDryer::EventKind`

`Info`, `Warning`, `Error`

### `iDryer::RequestKind`

`Start`, `Stop`, `Storage`, `Find`, `ClearErrors`

### `iDryer::IntegrationKind`

`Ha`, `Bambu`, `Moonraker`

### `iDryer::IntegrationState`

`Disabled`, `Idle`, `Connecting`, `Online`, `ConfigMissing`, `Error`

---

## When to go deeper

The facade is sufficient for most tasks. If you need to work below the facade level — with `idryer::IdryerRuntime`, `idryer::MqttClient`, `idryer::cloud::LinkIntegrationsManager` — see the Architecture section.
