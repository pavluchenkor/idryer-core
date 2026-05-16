# 故障排除

Common symptoms when working with `idryer-core`, their causes, and solutions.

Before reading, make sure HAL logs are enabled (`idryer::hal::initArduinoHal(&Serial)`) and that `-DCORE_DEBUG_LEVEL=3` or higher is set in `platformio.ini`.

## WiFi

### State machine stuck in `WifiConnecting`

Symptoms: log repeats `state: WifiConnecting`, transition to `Provisioning` never happens.

Possible causes:

- Incorrect SSID/password. Check `WIFI_SSID` / `WIFI_PASSWORD` in `secrets.h`. After Improv provisioning, credentials come from NVS, not from `secrets.h`.
- 5 GHz network. ESP32 supports 2.4 GHz only.
- Hidden network or MAC filter on the router.
- `WiFi.begin()` called before `idryer::hal::initArduinoHal(...)` — no log output, but this is not the cause of the hang, just blindness.

What to check:

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFi connects but drops after 30–60 seconds

Typically: weak signal (`RSSI < -80 dBm`), ESP32-C3 powered from a USB hub without a dedicated 5V/1A supply, conflict with FreeRTOS tasks.

Log RSSI in the product loop:

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## 配置和聲明

### State machine stuck in `Provisioning`

Symptoms: `state: Provisioning` without transitioning to `Registering` or `AwaitingClaim`.

Causes:

- Incorrect `IDRYER_API_BASE` in build_flags. Must be `https://portal.idryer.org/api` (production) or `https://staging.idryer.org/api` (staging).
- Missing TLS certificate (Let's Encrypt ISRG Root X1). Embedded in `root_ca.h`, but when built without `MQTT_USE_TLS`, the HTTP client also uses TLS — the root CA is needed for the HTTP API too.
- Device time not synchronized (TLS handshake requires a valid date). Check that `configTime(...)` is called in `setStateChangeCallback` after `WifiConnecting` (as in Storage Link).

### State machine stuck in `AwaitingClaim`

This is the normal state while the user has not entered the PIN in the portal. The PIN is printed to the log via `setClaimPinCallback`.

For automatic claiming (standalone devices without UI):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

After `requestClaim()`, the backend issues a PIN that the user must enter in the portal.

### `seedSerialFromMac()` generated a serial, but a different one was entered in the portal

The serial stored in NVS takes priority over MAC generation. `seedSerialFromMac()` writes to NVS only if no serial is present yet. To change the serial, clear NVS:

```cpp
s_credentials.clear();
```

## MQTT

### State machine entered `MqttConnecting` but does not reach `Online`

Causes:

- Broker unreachable. Production: `mqtt.idryer.org:8883`, staging: `staging.idryer.org:1884`.
- `MQTT_USE_TLS=1` without a correct root CA — handshake fails silently.
- `setBufferSize(16384)` not applied — `PubSubClient` buffer is 256 bytes by default. `MqttClient` already sets 16384, but if you use `PubSubClient` directly — set the buffer yourself.
- Persistent session "stuck" on the broker with a different client ID. Clear NVS and re-flash.

### Commands from the backend are not arriving

Check the subscription — `MqttClient` subscribes to `idryer/{serial}/commands/#` with QoS 1. If the subscription failed, the log will show:

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

Verify that `setCommandHandler()` is called **before** `runtime.begin()` — otherwise the first batch of commands may be missed.

### `PubSubClient` disconnects at exactly 60-second intervals

This is a keep-alive timeout. Your MQTT loop may not be called frequently enough — `s_runtime.loop()` must spin without long blocks. Check that `loop()` has no `delay(>500ms)` and no blocking network calls.

## 命令和處理程序

### `commands/invoke` arrives but `ActionDispatcher` is not called

If you registered `setCommandHandler()`, **the built-in fallback to `ActionDispatcher` is disabled**. `IdryerRuntime` passes everything (except `ping`) to your `CommandHandler`. You must explicitly call `s_dispatcher.handleInvoke(data)` there for `invoke` commands.

Template:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... product commands ...
}
```

### `commands/set` received but config was not applied

`ActionDispatcher::handleSet` extracts `id` and `val` and passes them to the registered `SetCallback`. Check that:

- `dispatcher.setSetCallback(onSetCommand, nullptr)` is called in `setup()`.
- `onSetCommand` actually calls `s_profile.applyConfig(id, val)`.
- `applyConfig` returns `true` for known `id` values. For unknown ones it returns `false` and changes are ignored.

## 遙測

### Telemetry is not published

`idryer-core` does not publish telemetry automatically. The product code always does this.

Check that:

- `pub.publishTelemetry(doc)` (or `s_mqtt.publishTelemetry(doc)` if LocalAccess is not used) is actually called in `loop()`.
- The rate condition is not blocking all calls. A common mistake:
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  On the first pass `lastTm == 0` and `millis()` is still small — the branch never executes. Use `>=` and initialize `lastTm` on the first pass.
- `s_runtime.isOnline() == true`. MQTT is disconnected before Online — publishing will not go through.
- `JsonDocument` size is sufficient for the payload. Check `doc.overflowed()` after `serializeJson`.

### `publishTelemetry` returns `false`

Causes:

- Not connected to the broker (`MqttClient::isConnected() == false`).
- Buffer exceeded — payload larger than `MQTT_BUFFER_SIZE` (16384 bytes). For large data use `publishConfigRaw` (with chunks) or reduce the payload.

### `DevicePublisher::publishTelemetry` does not reach the WS client

`DevicePublisher` does not return an error if the WS client is not connected — it simply skips the WS part. Check `s_local.isClientConnected()`. If `false` — the client is not authenticated or not connected.

## NTP 和系統時間

### Device time is not synchronized

NTP synchronization is started in `setStateChangeCallback` after the first exit from `WifiConnecting`:

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

If this callback is not registered — time is not synchronized automatically. A TLS handshake to the broker requires valid time; otherwise the certificate is considered expired or from the future.

Alternative channel: `IdryerRuntime` handles `commands/ping` and applies `data["timestamp"]` via `settimeofday()`. If the backend sends ping once per minute — time is updated without NTP.

### TLS handshake fails after long uptime

If the NTP server is unreachable and the device runs without reboot for a long time, time may drift (especially on ESP32-C3 without TCXO). Symptom: sudden `connection failed` after several days of uptime.

Solution: ensure `pool.ntp.org` is reachable from your network, or receive `commands/ping` from the backend more frequently.

### `getIsoTimestamp` returns year 1970

System time is not yet synchronized. Time appears after the first successful `configTime` or `commands/ping`. Until then, `info`/`telemetry` will be published with a placeholder.

## ArduinoJson

### Compile error: `StaticJsonDocument` is not a member of `ArduinoJson`

You are using ArduinoJson v7. The `StaticJsonDocument` type exists only in v6. Solutions:

- Pin v6 in `platformio.ini`:
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- Or migrate your code to the v7 API (`JsonDocument` instead of `StaticJsonDocument<N>`). `idryer-core` is written for v6.

### Compile error: ambiguous overload or type mismatch

Two versions of ArduinoJson may end up in one project through transitive dependencies. Check:

```bash
pio pkg list -e my-device | grep -i arduinojson
```

There must be **one** version. If there are two — pin it explicitly via `lib_deps` and if needed via `lib_ldf_mode = chain+` or `lib_ignore`.

### `doc.overflowed()` true after serializeJson

The `StaticJsonDocument<N>` size is too small for the payload. Increase `N` or use `DynamicJsonDocument` for infrequently called paths.

## 本地 WS (LocalAccess)

### App does not discover the device on LAN

mDNS should be started **immediately after the serial number is available** via `s_local.initMdns(serial)`. Check that:

- The router does not block multicast.
- The app is looking for `_idryer._tcp` on port 81.
- The device serial number matches what is registered in the portal.

### WS client connected but receives `auth_required`

The first message from the client must be `{"type":"auth","token":"<device_token>"}`. If the token is invalid, `LocalAccess` calls `setTokenRefreshCallback()`. The product must in that callback re-read the token from `ICredentialStore` and call `s_local.updateToken(...)`.

## 內存和穩定性

### Free heap decreases over time

`PubSubClient::loop()` and `WebSocketsServer::loop()` should not leak, but check your product code:

- Create `JsonDocument` on the stack (`StaticJsonDocument<N>`), not on the heap (`DynamicJsonDocument`) for frequently called paths.
- `String` in product code on ESP32-C3 quickly fragments the heap — use `char[]` and `snprintf`.

### `Stack overflow` or `Guru Meditation`

`s_runtime.loop()` does not spawn FreeRTOS tasks — everything runs in the Arduino loop. If there is a stack crash, look for:

- Large local `JsonDocument`/`char[8192]` on the Arduino loop stack (default 8 KB).
- Deep recursion in product code.

Increase the Arduino loop stack:

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi (provisioning via Serial)

### Improv does not accept credentials

Improv must own `Serial` until credentials are received:

```cpp
idryer::hal::initArduinoHal(nullptr);   // logs to /dev/null while Improv holds Serial
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // restore log output
}
```

If `HAL_LOG_*` writes to `Serial` in parallel with the Improv protocol, Improv fails on checksum.

### Improv client does not see the device

Check `ChipFamily` in `setDeviceInfo`. Must match the actual chip: `CF_ESP32_C3`, `CF_ESP32_S3`, `CF_ESP32_S2`, `CF_ESP32`. A mismatch — the Improv client will not show the device in the list.

Also ensure that the Serial baudrate is 115200. The Improv protocol expects this.

## 集成診斷

### Full diagnostic output (1 Hz)

Menu → `DIAGNOSTICS → DIAG LOG` (`menu.diag_en`). Disabled by default.
Enable via the device UI, portal (`commands/set` with `bind=diag_en`),
or REPL (`set diag_en 1`).

When enabled, a block is printed to Serial once per second:

```
=========== iHeater Link diagnostics ===========
[device]    serial=DEVICE_... online=1 uptime=42s
[wifi]      status=3 ssid=Apart_4 ip=192.168.0.140 rssi=-51
[rmt-out]   mode=DRYING target=70.0°C
[active]    bambu
[bambu]     state=CONNECTED  ip=192.168.0.171 serial=<set> lan=<set>
            gcode_state='RUNNING' tray='PLA' chamber_target=0.0 chamber_temp=0.0
[moonraker] state=DISABLED   ws=ws://192.168.0.171:7125
            vc.available=0 vc.target=0.0 vc.temp=0.0 vc.has_sensor=0
[ha]        state=DISABLED   host=<empty>:1883 user=<empty>
[menu]      bambu_en=1 moon_en=0 ha_en=0 diag_en=1  mat_pla=45 ...
================================================
```

Useful for remote diagnostics: the user enables `DIAG LOG`, copies the
output → connector states, lastError, and what is actually going to RMT are visible.

### ANOMALY channel (event-based)

Independently of `diag_en`, connectors and helpers write separate lines with
the prefix `[!] ANOMALY` on unexpected conditions:

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

The `[!]` prefix visually highlights the anomaly in the general log stream. This is the first thing to look for in Serial when something "is not working".

### Auto-OFF on connection loss (fail-safe)

If the active integration loses its connection (TCP/WS disconnect), the connector
immediately resets the target temperature:

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`, `available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF.
- **Bambu** — transition `Connected → !Connected` → `chamberTarget=0`, `trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF.
- **HA** — fail-safe not yet implemented.

Without this logic, heating would continue at the last known target until
the connection is restored.

### Bambu: gcode_state filter

`auto_heat` heats **only** when `gcode_state == "RUNNING"` or `"PREPARE"`.
All other states (`IDLE`, `FINISH`, `FAILED`, `PAUSE`, `INIT`, `OFFLINE`,
`SLICING`, `UNKNOWN`, empty) → OFF.

When diagnosing, pay attention to the `gcode_state` in the `[bambu]` diagnostics line — if it shows `IDLE`/`FINISH`, there will be no heating regardless of whether an active tray is present.

### Test benches for debugging without a printer

For testing integrations without real printers, product
repositories (e.g., iHeater-link) may contain stub utilities
such as `fake_moonraker` / `fake_bambu` that send a ramp of values
every 30 seconds.
