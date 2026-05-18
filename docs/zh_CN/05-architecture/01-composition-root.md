# 组合根

产品在 `main.cpp` 中创建所有库对象作为静态变量，并通过构造函数传递依赖项。没有工厂，没有全局注册表——仅显式组装。

## 对象创建顺序

依赖项从下到上构建：平台层先，然后云堆栈，然后运行时。

```cpp
// 1. 平台层
idryer::ArduinoWifiStore       s_wifiStore;      // NVS: SSID/password
idryer::ArduinoWifiManager     s_wifi;           // WiFi 管理
idryer::ArduinoCredentialStore s_credentials;    // NVS: serial/token/deviceId
idryer::ArduinoHttpClient      s_http;           // TLS HTTP 用于配置

// 2. 云堆栈
idryer::cloud::HttpApi           s_api(&s_http, IDRYER_API_BASE);
idryer::MqttClient               s_mqtt;
idryer::cloud::CloudStateMachine s_cloud(&s_wifi, &s_credentials, &s_api, &s_mqtt);
idryer::ActionDispatcher         s_dispatcher;

// 3. 产品配置文件（实现 IProfile）— 产品代码，不是库
LedStripProfile s_profile(&s_executor);

// 4. 运行时 — 把一切联系在一起
idryer::IdryerRuntime s_runtime(&s_cloud, &s_dispatcher, &s_profile, &s_mqtt);
```

## setup() 做什么

```cpp
void setup() {
    // HAL: 日志进入 /dev/null 当 Improv 拥有 Serial
    idryer::hal::initArduinoHal(nullptr);

    // 恢复保存的 WiFi 凭据
    char ssid[64], pass[64];
    if (s_wifiStore.load(ssid, sizeof(ssid), pass, sizeof(pass))) {
        s_wifi.begin(ssid, pass);
    }

    // 如果还没有，从 MAC 生成序列号
    s_credentials.seedSerialFromMac();

    // 注册命令处理程序
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_dispatcher.setSetCallback(onSetCommand, nullptr);

    // 可选：对状态机转换作出反应
    s_cloud.setStateChangeCallback([](auto prev, auto, void*) {
        if (prev == idryer::cloud::CloudState::WifiConnecting)
            configTime(0, 0, "pool.ntp.org", "time.google.com");
    }, nullptr);

    // 对未声称设备的自动声称
    s_cloud.setUnclaimedCallback([](void*) {
        s_cloud.requestClaim();
    }, nullptr);

    // 启动运行时
    s_runtime.begin();
}

void loop() {
    s_runtime.loop();     // CloudStateMachine + IProfile::loop()
    // ... 产品逻辑（传感器、遥测）
}
```

## 组装规则

- 所有库对象都是静态（`static`）。没有 `new` 或 `malloc` 用于顶级对象。
- `runtime.begin()` 在 `setup()` 中最后调用，在所有处理程序注册之后。
- `runtime.loop()` 在 `loop()` 中第一个调用。
- 产品对象（传感器、遥测）被分别创建并直接连接到 `s_mqtt` — 运行时不知道它们。

## 示例：存储链接

完整的存储链接组合根在 iDryer-Storage 仓库中的 `src/main.cpp` 中（分开发布）。

组装顺序中的设备层：

| 层 | 对象 | 来源 |
|-----|------|------|
| 平台 | `s_wifiStore`、`s_wifi`、`s_credentials`、`s_http` | `idryer-core` |
| 云 | `s_api`、`s_mqtt`、`s_cloud`、`s_dispatcher` | `idryer-core` |
| 设备 | `s_executor`、`s_profile` | `src/storage/led_strip/` |
| 运行时 | `s_runtime` | `idryer-core` |
| 传感器 | `s_sensor`、`s_telemetry` | `src/storage/sensors/`、`src/storage/telemetry/` |
