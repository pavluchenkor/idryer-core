# モードなしのアクション：リレーとデバイス呼び出し

このページを終えると、カードに分単位の時間を入れる **Ventilate** フォームと、ヘッダーにデバイス呼び出しボタンが表示されます。

## 必要なもの

- 3.3 V で制御できるリレーモジュール（またはトランジスタスイッチ）をピン 5 に；
- リレーにつなぐファンなどの負荷。

!!! warning
    230 V の商用電源の負荷には、絶縁、ヒューズ、ケースが必要です。商用電源の経験がなければ、低電圧の負荷で組んでください。

## コード

```cpp
#include <Arduino.h>
#include <iDryer.h>

#define RELAY_PIN 5
#define LED_PIN   8

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasFan          = true,    // 「ファン オン/オフ」のセル
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Vent Relay",
};
static iDryer::Link s_link(CFG);

static uint32_t s_ventUntilMs = 0;   // 0 はリレーがオフ
static uint8_t  s_blinks      = 0;

static void onVent(uint8_t, JsonObjectConst args) {
    const uint32_t minutes = args["duration"].as<uint32_t>();   // 1..60
    s_ventUntilMs = millis() + minutes * 60000UL;
    digitalWrite(RELAY_PIN, HIGH);
    s_link.telemetry.fanOn[0] = true;   // オン/オフの変化はすぐに送信される
}

static void onIdentify(uint8_t, JsonObjectConst) { s_blinks = 10; }

static void tick() {   // 250 ms ごと
    if (s_ventUntilMs && (int32_t)(millis() - s_ventUntilMs) >= 0) {
        s_ventUntilMs = 0;
        digitalWrite(RELAY_PIN, LOW);
        s_link.telemetry.fanOn[0] = false;
    }
    if (s_blinks) digitalWrite(LED_PIN, --s_blinks % 2);
}

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    // モードなし：ユニットは「使用中」にならず、フォームはカードに残る。
    card.action("vent", nullptr, onVent)
        .name("ru", "Проветрить").name("en", "Ventilate")
        .param("duration", "duration", 1, 60, 1, 5, "min");
    // カードのヘッダーにあるデバイス呼び出しボタン。
    card.action("identify", nullptr, onIdentify).deviceClass("identify");

    s_link.every(250, tick);
}

void loop() {
    s_link.loop();
}
```

## 仕組み

- モードなしのアクション（`mode` = `nullptr`）はユニットを「使用中」にしません。カードにセッションのブロックは出ず、フォームはそのまま残ります。
- purpose の `duration` は分単位の時間です。項目には **時間** のラベルが付き、範囲は `.param(...)` から取られます。
- `hasFan` で「ファン オン/オフ」のセルが出ます。`telemetry.fanOn` の変化はコアが周期を待たずにすぐ送信します。
- `.deviceClass("identify")` はカードのヘッダーに常に出るデバイス呼び出しボタンで、`clear_errors` はエラーのリセットです。

## 確認

カードで時間を設定し **Ventilate** をタップすると、リレーがオンになり、ファンのセルに「オン」と表示されます。ヘッダーの呼び出しボタンでボードの LED が点滅します。

## 次へ

[PWM による加熱](06-pwm.md)。
