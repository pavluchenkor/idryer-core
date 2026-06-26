# 组合根

产品在 `main.cpp` 中把所有库对象创建为静态变量，并通过构造函数传递依赖。没有工厂、没有全局 registry，只有显式组装。

## 对象创建顺序

依赖从底向上构建：先平台层，再云端栈，最后 runtime。

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

## 组装规则

- 所有库对象都是静态的（`static`）。顶层对象不要使用 `new` 或 `malloc`。
- `runtime.begin()` 在 `setup()` 中最后调用，且应在所有 handler 注册之后。
- `runtime.loop()` 在 `loop()` 中最先调用。
- 产品对象（传感器、遥测等）单独创建，并直接连接到 `s_mqtt`；runtime 不需要知道它们。

## 示例：Storage Link

完整的 Storage Link 组合根位于 iDryer-Storage 仓库中的 `src/main.cpp`（该仓库单独发布）。

按组装顺序划分的设备层：

| 层 | 对象 | 来源 |
|-------|---------|--------|
| Platform | `s_wifiStore`, `s_wifi`, `s_credentials`, `s_http` | `idryer-core` |
| Cloud | `s_api`, `s_mqtt`, `s_cloud`, `s_dispatcher` | `idryer-core` |
| Device | `s_executor`, `s_profile` | `src/storage/led_strip/` |
| Runtime | `s_runtime` | `idryer-core` |
| Sensors | `s_sensor`, `s_telemetry` | `src/storage/sensors/`, `src/storage/telemetry/` |
