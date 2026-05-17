# 参加者間のデータフロー

適用セクション：センサー、ペリフェラル、プロファイル、トランスポート、パブリッシャーが実際のプロダクトコード内でどのように接続されているかについて。アーキテクチャ的なデータフロー説明は [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) にあります。

## 原則

`idryer-core` は意図的に内部イベントバスを提供しません。参加者間のすべての接続は、コンポジション ルート内でコンストラクターを通じて渡される **明示的なポインター** です。これは以下を意味します：

- すべてのデータフローは `main.cpp` 内のポインターチェーンとして読むことができます。
- 「魔法の」参加者検出はありません。
- プロダクトは誰が何を誰に渡すかを決定します。

## Storage Link の典型的な接続マップ

```
   Sensor (Sht31ClimateSensor)
        │
        │ tick(now), get()
        ▼
   StorageTelemetryPublisher    ──→  DevicePublisher  ──→  MqttClient + LocalAccess
                                                            │
                                                            ▼
                                                       broker / WS-client


   handleCommand   ←──  IdryerRuntime   ←──  MqttClient (commands/*)
        │           ←──  LocalAccess    ←──  WS-client (envelope)
        │
        ├──→  ActionDispatcher  ──→  LedStripExecutor (peripheral)
        ├──→  IProfile::getConfig  ──→  DevicePublisher::publishConfig
        └──→  IProfile::applyConfig (via onSetCommand)
```

各矢印は `main.cpp` の 1 つのポインター渡し行です。例えば：

```cpp
static Sht31ClimateSensor        s_sensor(&Wire);
static StorageTelemetryPublisher s_telemetry(&s_sensor, &s_pub);
//                                            ^^^^^^^^   ^^^^^
//                                            sensor     publisher
```

## レシピ 1 — センサーがクラウドに発行する

**目標**：温度センサー → MQTT。

```
Sensor → Publisher → DevicePublisher → MqttClient + LocalAccess
```

```cpp
static MySensor              s_sensor;
static MyTelemetryPublisher  s_telemetry(&s_sensor, &s_pub);

void loop() {
    s_runtime.loop();
    s_local.loop();
    s_sensor.tick(millis());
    s_telemetry.loop(millis());
}
```

`MyTelemetryPublisher::loop` は発行時期を決定します（インターバルで）。[01-add-sensor.md](01-add-sensor.md) を参照してください。

## レシピ 2 — クラウドコマンド → ペリフェラル

**目標**：`commands/invoke {"action":"led.pulse",...}` → LED をオンにする。

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onInvoke → LedStripExecutor
```

```cpp
static bool onInvoke(const char* action, JsonObjectConst args, void* /*ctx*/) {
    return s_executor.execute(action, args);
}

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    // ...
}

void setup() {
    s_dispatcher.setInvokeHandler(onInvoke, nullptr);
    s_runtime.setCommandHandler(handleCommand);
    // ...
}
```

[02-add-peripheral.md](02-add-peripheral.md) を参照してください。

## レシピ 3 — LAN アプリコマンド → ペリフェラル（同じパス）

**目標**：LAN上のWSクライアントが `{"type":"command","command":"invoke","data":{"action":"led.pulse",...}}` を送信 → 同じLEDがオンになる。

```
WS-client → LocalAccess → CommandSink → handleCommand → ActionDispatcher → ...
```

新しいコードは不要です。`s_local.setCommandSink(handleCommand)` は既に両方のトランスポートを 1 つのハンドラーにマージします。

## レシピ 4 — センサー → ペリフェラル（内部ループ）

**目標**：センサーが湿度を読み取ります → 閾値を超えた場合、ファンがオンになります。

これはプロダクト内部ロジックです。`idryer-core` にはそのような接続用のAPIはありません。直接行います：

```cpp
class HumidityController {
public:
    HumidityController(IClimateSensor* sensor, Fan* fan, float threshold)
        : sensor_(sensor), fan_(fan), threshold_(threshold) {}

    void loop(uint32_t nowMs) {
        if (nowMs - lastCheckMs_ < 5000) return;
        lastCheckMs_ = nowMs;

        SensorReading r = sensor_->get();
        if (!r.ok) return;
        if (r.humidity > threshold_)  fan_->on();
        else                          fan_->off();
    }
private:
    IClimateSensor* sensor_;
    Fan*    fan_;
    float           threshold_;
    uint32_t        lastCheckMs_ = 0;
};
```

コンポジション ルートで接続：

```cpp
static HumidityController s_humCtrl(&s_sensor, &s_fan, 60.0f);

void loop() {
    s_runtime.loop();
    s_sensor.tick(millis());
    s_humCtrl.loop(millis());
}
```

`idryer-core` はこのクラスについて知りません。また知るべきではありません。

## レシピ 5 — 設定変更 → ペリフェラル再初期化

**目標**：バックエンドが `commands/set {"id":CFG_BRIGHTNESS,"val":150}` を送信 → LED の明るさが即座に変わります。

```
MqttClient → IdryerRuntime → handleCommand → ActionDispatcher → onSetCommand → IProfile::applyConfig → Peripheral
```

```cpp
class MyProfile : public idryer::IProfile {
public:
    MyProfile(MyDevice* a) : device_(a) {}

    bool applyConfig(int id, int val) override {
        if (id == CFG_BRIGHTNESS) {
            menu.brightness = val;
            menu.saveToNVS();
            device_->setBrightness(val);   // immediate apply
            return true;
        }
        return false;
    }
    // ...
private:
    MyDevice* device_;
};
```

`profile → peripheral` の接続はコンポジション ルートで構築されます：

```cpp
static MyDevice s_device;
static MyProfile  s_profile(&s_device);
```

## レシピ 6 — 新しいイベント → events トピック

**目標**：ペリフェラルがエラーをキャッチ → `idryer/{serial}/events` 内のイベント。

ペリフェラルは単独で発行しません。プロダクトに通知します。プロダクトが発行します：

```cpp
class MyDevice {
public:
    using ErrorCallback = std::function<void(int errCode, const char* msg)>;
    void setErrorCallback(ErrorCallback cb) { errCb_ = cb; }
    // ...
private:
    ErrorCallback errCb_;
    void reportError(int code, const char* msg) {
        if (errCb_) errCb_(code, msg);
    }
};

// in main.cpp
s_device.setErrorCallback([](int code, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["code"] = code;
    doc["msg"]  = msg;
    s_pub.publishEvent(doc);
});
```

別の方法として、ペリフェラルはコンストラクターを通じて `DevicePublisher*` を受け取ることもできます。重要なポイント：接続は明示的です。

## 実装しないもの

- 内部イベントバスは導入しません。これは隠れた接続とデバッグの複雑性につながります。
- センサー/ペリフェラル/パブリッシャーを共有 `IDeviceContainer` に収集しません。接続はコンポジション ルートで正確に構築されます。
- 名前ベースのサブスクリプション（「パブリッシャー 'telemetry' がセンサー 'sht31' をリッスンする」）を使用しません。すべての接続は型付きポインターです。

## 関連ドキュメント

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — 作成とアセンブリ順序。
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — アーキテクチャ図。
- [04-patterns/01-add-sensor.md](01-add-sensor.md)、[02-add-peripheral.md](02-add-peripheral.md)、[03-add-transport.md](03-add-transport.md) — 具体的なコンポーネント レシピ。
