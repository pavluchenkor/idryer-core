# ステップ 05 — ポータル コマンド: RMT 出力

このステップの後、ポータルの開始ボタンを押すと、ESP32 出力ピンに RMT パルスが生成されます。この例は iHeater Link に従います。ここではピンはオプトカプラー経由で iHeater STM32 を駆動します。

## 仕組み

ポータルは `invoke` コマンドを MQTT トピック `idryer/{serial}/commands/invoke` に発行します。ライブラリは JSON をデシリアライズし、登録されたハンドラーを呼び出します。ハンドラーはコマンドを `RmtOutputAdapter` に渡します。これは選択されたピンに RMT フレームを生成します。

ハンドラーは特定のピンまたはプロトコルに独立しています — それは単純なコールバック関数です。RMT は 1 つの実装です; PWM は別のもので、[06-pwm.md](06-pwm.md) を参照してください。

## 必要なもの

- ESP32-C3 または ESP32 (RMT はすべての GPIO ピンで利用可能)
- 出力ピンの負荷 (iHeater Link — オプトカプラー経由の STM32)

## 手順

**1. `main.cpp` で RmtOutputAdapter を宣言します。** [`iHeater-link/src/main.cpp`](../../../../iHeater-link/src/main.cpp) に基づいています:

```cpp
#include "controller/RmtOutputAdapter.h"

static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

デフォルト出力ピンは `IHEATER_TRIGGER_OUTPUT_PIN` です。`build_flags` 経由で設定します:

```ini
build_flags =
    -DIHEATER_TRIGGER_OUTPUT_PIN=0
```

**2. `setup()` で初期化します:**

```cpp
s_output.begin();
```

`begin()` は RMT チャネルを構成し、キープアライブ フレームを送信するバックグラウンド FreeRTOS タスクを開始します。

**3. `setup()` でコマンド ハンドラーを登録します:**

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action    = data["action"] | "";
    JsonObjectConst args  = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float    tempC  = args["tempC"]      | 0.0f;
        uint32_t durMin = args["durationMin"] | 0u;

        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::TargetTemperature;
        cmd.targetTempC = tempC;
        s_output.apply(cmd);

        device().status.mode[0]        = iDryer::UnitMode::Drying;
        device().status.targetTempC[0] = tempC;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        iheaterlink::ControllerOutputCommand cmd;
        cmd.mode        = iheaterlink::ControllerOutputMode::Off;
        cmd.targetTempC = 0.0f;
        s_output.apply(cmd);

        device().status.mode[0] = iDryer::UnitMode::Idle;
        device().publishStatusNow();
    }
});
```

**4. `loop()` で — `device().loop()` のみを呼び出します:**

```cpp
void loop() {
    device().loop();
}
```

RMT フレームは `s_output` 内の FreeRTOS タスクから、`loop()` に依存せずに送信されます。

## ポータルがコマンドを送信する方法

ポータルは MQTT トピック `idryer/{serial}/commands/invoke` に発行します:

```json
{
  "action": "heat.start",
  "args": { "tempC": 55.0, "durationMin": 120 }
}
```

ライブラリはこのメッセージを受け取り、デシリアライズされた `JsonObjectConst data` で登録されたコールバックを呼び出します。`action` フィールドが何をするかを決定します。

各デバイス タイプのアクション リストは [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) で `invoke_actions` の下で定義されています。

## 検証

ポータル → デバイス ページ → **ヒート** ボタンを押します。Serial Monitor:

```
[CMD] invoke:heat.start temp=55.0 duration=7200s
```

RMT パルスは出力ピンに表示されます (オシロスコープまたはロジック アナライザーで検証します)。

## 次は?

- [06-pwm.md](06-pwm.md) — RMT を PWM に置き換えます (MOSFET、DC ディマー)。
- [RmtOutputAdapter.h](../../../../iHeater-link/src/controller/RmtOutputAdapter.h) — RMT 設定: パルス周波数、Off コード、温度範囲。
