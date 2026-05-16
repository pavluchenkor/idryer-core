# コンポジション ルート

プロダクトはすべてのライブラリオブジェクトを `main.cpp` に静的変数として作成し、コンストラクターを通じて依存関係を渡します。ファクトリーも グローバルレジストリーもありません。明示的なアセンブリのみです。

## オブジェクト作成順序

依存関係はボトムアップで構築されます：プラットフォーム層、その後クラウドスタック、その後ランタイム。

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

## setup() の処理内容

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

## アセンブリ ルール

- すべてのライブラリオブジェクトは静的（`static`）です。トップレベルオブジェクトには `new` や `malloc` はありません。
- `runtime.begin()` は `setup()` の最後で呼ばれます（すべてのハンドラーが登録された後）。
- `runtime.loop()` は `loop()` の最初で呼ばれます。
- プロダクトオブジェクト（センサー、テレメトリ）は別途作成され、`s_mqtt` に直接接続されます。ランタイムはそれらを知りません。

## 例：Storage Link

完全な Storage Link コンポジション ルートは
iDryer-Storage リポジトリ（別個に発行）の `src/main.cpp` にあります。

アセンブリ順序のデバイス層：

| 層 | オブジェクト | ソース |
|-------|---------|--------|
| Platform | `s_wifiStore`, `s_wifi`, `s_credentials`, `s_http` | `idryer-core` |
| Cloud | `s_api`, `s_mqtt`, `s_cloud`, `s_dispatcher` | `idryer-core` |
| Device | `s_executor`, `s_profile` | `src/storage/led_strip/` |
| Runtime | `s_runtime` | `idryer-core` |
| Sensors | `s_sensor`, `s_telemetry` | `src/storage/sensors/`, `src/storage/telemetry/` |
