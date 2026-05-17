# ステップ 06 — RMT を PWM に置き換える

このステップの後、同じポータル コマンド フローが RMT の代わりに PWM 出力を駆動します。典型的なユース ケースは、MOSFET または DC ディマーで制御されるヒーターです。

## 仕組み

実行器は単純なコールバック関数です。前のステップの `RmtOutputAdapter` は 1 つの実装です。`ledcWrite` コードに置き換えます — その他すべて (MQTT、コマンド、ステータス) は変わりません。

## 手順

**1. `main.cpp` から `RmtOutputAdapter` include と instance を削除します:**

```cpp
// Remove:
#include "controller/RmtOutputAdapter.h"
static iheaterlink::RmtOutputAdapter s_output{iheaterlink::RmtOutputConfig{}};
```

**2. `setup()` で PWM 初期化を追加します:**

```cpp
#define PWM_PIN     0      // GPIO for MOSFET gate
#define PWM_CHANNEL 0      // LEDC channel (0–15)
#define PWM_FREQ_HZ 25000  // 25 kHz — inaudible for most heaters
#define PWM_RES     8      // 8-bit → duty 0–255

ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RES);
ledcAttachPin(PWM_PIN, PWM_CHANNEL);
ledcWrite(PWM_CHANNEL, 0);  // off at startup
```

**3. コマンド ハンドラーで** `s_output.apply(cmd)` を `ledcWrite` に置き換えます:

```cpp
device().onCommand("invoke", [](JsonObjectConst data) {
    const char* action   = data["action"] | "";
    JsonObjectConst args = data["args"];

    if (strcmp(action, "heat.start") == 0) {
        float power01 = args["power"] | 1.0f;  // 0.0–1.0
        uint8_t duty  = (uint8_t)(power01 * 255.0f);
        ledcWrite(PWM_CHANNEL, duty);

        device().status.mode[0]             = iDryer::UnitMode::Drying;
        device().telemetry.heaterPower01[0] = power01;
        device().publishStatusNow();

    } else if (strcmp(action, "heat.stop") == 0) {
        ledcWrite(PWM_CHANNEL, 0);

        device().status.mode[0]             = iDryer::UnitMode::Idle;
        device().telemetry.heaterPower01[0] = 0.0f;
        device().publishStatusNow();
    }
});
```

**4. `loop()` は変わりません:**

```cpp
void loop() {
    device().loop();
}
```

!!! warning
    `ledcSetup` / `ledcAttachPin` は 3.x より前のバージョンの Arduino ESP32 API です。バージョン 3.x 以上では `ledcAttach(pin, freq, resolution)` と `ledcWrite(pin, duty)` を使用します。`platformio.ini` でバージョンを確認します (`platform = espressif32@X.Y.Z`)。

## 検証

ポータルの **ヒート** ボタンを押します。出力ピンは `power` 引数に比例したデューティ サイクルを持つ PWM 信号を伝送します。マルチメーター (平均電圧) またはオシロスコープで検証します。

## 次は?

- [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md) — 完全な `iDryer::Link` API リファレンス。
- [../04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md) — 任意の新しいアクチュエータのテンプレート。
