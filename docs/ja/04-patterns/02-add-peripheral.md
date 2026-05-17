# ペリフェラルを追加する

## 使用する場合

デバイスがクラウドまたはLANからのコマンドでハードウェアを制御する必要がある場合（リレー、ヒーター、LEDストリップ、モーターなど）、このレシピを使用します。

## すぐに使えるコード

```cpp
// main.cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::StorageLink,
    .unitsCount      = 1,
    .hardwareVersion = "1.0",
    .firmwareVersion = "1.0.0",
};

static iDryer::Link s_link(CFG);

static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (!cmd) return;

    if (strcmp(cmd, "invoke") == 0) {
        const char* action = data["action"] | "";

        if (strcmp(action, "fan.on") == 0) {
            myFan.on();
            s_link.publishStatusNow();  // reflect new state immediately
            return;
        }
        if (strcmp(action, "fan.off") == 0) {
            myFan.off();
            s_link.publishStatusNow();
            return;
        }
    }

    if (strcmp(cmd, "drying") == 0) {
        float targetTempC  = data["targetTempC"]  | 45.0f;
        uint32_t durationS = data["durationS"]    | 0;
        myHeater.start(targetTempC, durationS);
        s_link.status.mode[0]        = iDryer::UnitMode::Drying;
        s_link.status.targetTempC[0] = targetTempC;
        s_link.status.durationS[0]   = durationS;
        s_link.publishStatusNow();
        return;
    }

    if (strcmp(cmd, "stop") == 0) {
        myHeater.stop();
        s_link.status.mode[0] = iDryer::UnitMode::Idle;
        s_link.publishStatusNow();
        return;
    }
}

void setup() {
    myFan.begin();
    myHeater.begin();
    s_link.begin();
    // IMPORTANT: setCommandHandler — strictly AFTER begin().
    // begin() installs its own dispatcher; our handleCommand must overwrite it.
    s_link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    s_link.loop();
    myFan.tick();
    myHeater.tick();
}
```

## 説明

`s_link.runtime()->setCommandHandler(handleCommand)` は、コマンドハンドラーの単一の接続ポイントです。この呼び出し後、すべての受信MQTT コマンド（`invoke`、`set`、`drying`、`stop`、`ping`、`get_config` など）は直接 `handleCommand` に到達します。

`s_link.publishStatusNow()` — `s_link.status.*` への変更後に毎回呼び出します。これにより、`statusPeriodMs` タイマーを待たずに新しい状態をポータルとLANクライアントに即座に送信します。

`handleCommand` 内で `delay()` を呼ばないでください。この呼び出しはMQTTコールバックから同期的であり、ブロックするとセッションが中断されます。タイマーはプロダクトオブジェクトの `loop()` に配置します。

### 代替案：`link.onRequest()`

標準コマンド（`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`）の場合、より単純な `onRequest()` コールバックで十分です。生JSONを解析する必要はありません：

```cpp
s_link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start:
            myHeater.start(r.targetTempC, r.durationS);
            break;
        case iDryer::RequestKind::Stop:
            myHeater.stop();
            break;
        default:
            break;
    }
});
```

`onRequest()` は `setCommandHandler` と同時には機能しません。フルハンドラーが設定されている場合、`onRequest` コールバックは呼ばれません。詳細は [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) を参照してください。

## リポジトリ内の完全な例

リファレンス実装：`iHeater-link/src/main.cpp` の `handleCommand` による `drying` / `stop` の処理。
