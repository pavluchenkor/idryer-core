# 組合根

The product creates all library objects in `main.cpp` as static variables and passes dependencies through constructors. No factories, no global registry — only explicit assembly.

## 對象創建順序

Dependencies are built bottom-up: platform layer first, then cloud stack, then runtime.

```cpp
// 1. Platform layer
idryer::ArduinoWifiStore       s_wifiStore;      // NVS: SSID/password
idryer::ArduinoWifiManager     s_wifi;           // WiFi management
idryer::ArduinoCredentialStore s_credentials;    // NVS: serial/token/deviceId
idryer::ArduinoHttpClient      s_http;           // TLS HTTP for provisioning

// 2. Cloud stack TODO: add purpose description
idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
idryer::MqttClient               s_mqtt;
idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
idryer::ActionDispatcher         s_dispatcher;

// 3. Product profile (implements IProfile) — product code, not the library
LedStripProfile s_profile(&s_executor);

// 4. Runtime — ties everything together
idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
```

## setup() 的作用

```cpp
void setup() {
    // HAL: logs go to /dev/null while Improv owns Serial
    idryer::hal::initArduinoHal(nullptr);

    // Restore saved WiFi credentials
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    }

    // Generate serial from MAC if not yet present
    s_credentials.seedSerialFromMac();

    // Register command handlers
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    // Optional: react to state machine transitions
    s_cloud.setStateChangeCallback([](auto prev, auto, void*) {
        if (prev == idryer::cloud::CloudState::WifiConnecting)
            configTime(0, 0, "pool.ntp.org", "time.google.com");
    }, nullptr);

    // Automatic claiming for standalone devices
    s_cloud.setUnclaimedCallback([](void*) {
        s_cloud.requestClaim();
    }, nullptr);

    // Start the runtime
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();     // CloudStateMachine + IProfile::loop()
    // ... product logic (sensors, telemetry)
}
```

## 組裝規則

- All library objects are static (`static`). No `new` or `malloc` for top-level objects.
- `runtime.begin()` is called last in `setup()`, after all handlers are registered.
- `runtime.loop()` is called first in `loop()`.
- Product objects (sensors, telemetry) are created separately and connected to `s_mqtt` directly — the runtime does not know about them.

## 示例：Storage Link

The full Storage Link composition root is in `src/main.cpp` in the
iDryer-Storage repository (published separately).

Device layers in assembly order:

| Layer | Objects | Source |
|-------|---------|--------|
| Platform | `s_wifiStore`, `s_wifi`, `s_credentials`, `s_http` | `idryer-core` |
| Cloud | `s_api`, `s_mqtt`, `s_cloud`, `s_dispatcher` | `idryer-core` |
| Device | `s_executor`, `s_profile` | `src/storage/led_strip/` |
| Runtime | `s_runtime` | `idryer-core` |
| Sensors | `s_sensor`, `s_telemetry` | `src/storage/sensors/`, `src/storage/telemetry/` |
