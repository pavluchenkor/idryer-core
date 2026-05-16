# プロファイル モデル

プロファイルは `IProfile` インターフェースの実装で、特定のデバイスの動作を説明します。ライブラリはこのインターフェース経由でのみプロダクトと相互作用します。

## IProfile インターフェース

```cpp
class IProfile {
public:
    virtual ~IProfile() = default;

    virtual void onOnline() = 0;
    virtual void loop() = 0;
    virtual void getConfig(JsonDocument& out) = 0;
    virtual bool applyConfig(int id, int val) = 0;
    virtual void buildInfoJson(char* buf, size_t len) const = 0;
};
```

### ライブラリが各メソッドを呼ぶ時期

| メソッド | 呼ばれるタイミング | 実行すべきこと |
|--------|------------|----------------|
| `onOnline()` | `CloudStateMachine` がオンライン状態に初めて遷移するとき | NVS から設定をロード、ハードウェアに適用 |
| `loop()` | `IdryerRuntime::loop()` の各イテレーション | タイマー、アニメーション、センサー ポーリング |
| `buildInfoJson(buf, len)` | オンライン状態に遷移するとき；`ping` で | デバイス情報ペイロードをシリアル化 |
| `getConfig(out)` | `invoke device.getConfig` で | 現在の設定で doc を埋める |
| `applyConfig(id, val)` | `commands/set` で | パラメーター を適用、NVS に保存 |

## 例：LedStripProfile

`LedStripProfile` は Storage Link のプロファイル実装です。`src/storage/led_strip/` に位置します。

```cpp
class LedStripProfile : public IProfile {
public:
    explicit LedStripProfile(LedStripExecutor* executor);

    void onOnline() override;
    void loop() override;
    void getConfig(JsonDocument& out) override;
    bool applyConfig(int id, int val) override;
    void buildInfoJson(char* buf, size_t len) const override;

    static void normalizeGroups();        // fix NVS state of toggle groups
    static uint8_t selectedStripType();   // 0=WS2812B, 1=APA102
    static uint8_t selectedColorOrder();  // 0=GRB, 1=RGB, 2=BRG, 3=BGR

    static constexpr const char* DEVICE_TYPE = "storage_link";
    static constexpr const char* HW_VERSION  = "1.0";
    static constexpr const char* FW_VERSION  = "1.0.0";

private:
    LedStripExecutor* executor_;
};
```

`onOnline()` は現在の LED ストリップ設定（LED カウント、明るさ）を `LedStripExecutor` に適用します。

`applyConfig(id, val)` は `menu_ids.h` から パラメータ ID と新しい値を受け入れます。NVS を通じて `menu` オブジェクトで保存します。`strip_type` と `color_order` のようなパラメーターはリブートが必要です。FastLED はスタートアップで一度だけ初期化されます。

`buildInfoJson` は `idryer/{serial}/info` のペイロードを構築します。フィールド構成はプロダクトで定義されます。Storage Link は以下を発行：

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

複数のチャンバー ユニット（iDryer LINK）を持つデバイスの場合、`workTimeCounter`、`unitsCount`、機能を説明する `units` 配列を追加するのが一般的です。

## ActionDispatcher

`ActionDispatcher` はヒープを節約するために std::function なしで2つのコマンド タイプをルーティング（プレーン関数ポインター）：

```cpp
// Invoke: action with name and arguments
using InvokeHandler = bool (*)(const char* action, JsonObjectConst args, void* ctx);

// Set: setting a single parameter
using SetCallback = void (*)(JsonObjectConst data, void* ctx);
```

`setup()` での登録：

```cpp
// Invoke — delegates to LedStripExecutor
dispatcher.setInvokeHandler(
    [](const char* action, JsonObjectConst args, void* /*ctx*/) -> bool {
        return s_executor.execute(action, args);
    }, nullptr);

// Set — passes id/val to LedStripProfile
dispatcher.setSetCallback(
    [](JsonObjectConst data, void* /*ctx*/) {
        int id  = data["id"]  | -1;
        int val = data["val"] | -1;
        s_profile.applyConfig(id, val);
    }, nullptr);
```

`IdryerRuntime` は対応するMQTTコマンドが到着したとき `dispatcher.handleInvoke(data)` と `dispatcher.handleSet(data)` を呼びます。

## 新しいプロファイルの作成

1. `IProfile` を継承するクラスを作成します。
2. 5つのメソッドすべてを実装します。
3. プロファイルへのポインターを `IdryerRuntime` コンストラクターに渡します。
4. `ActionDispatcher` の `invoke` と `set` コマンドのハンドラーを登録します。

プロファイルがそのメソッド内で何を行うかについては制限はありません。プロダクト コンテキストへの完全な可視性があります。
