# PWM による加熱：モード、セッション、出力

このページを終えると、カードから温度と時間を指定して加熱を始め、セッションと出力を表示できます。デバイスはセンサーに基づいて PWM 出力で温度を保ちます。

## 必要なもの

- [テレメトリのステップ](03-telemetry.md) の SHT31 センサー；
- ピン 3 にロジックレベル MOSFET（3.3 V で開く）と、その電源電圧に合うヒーター；
- `platformio.ini` に `robtillaart/SHT31 @ ^0.5.0`。

!!! warning
    温度ヒューズのないヒーターを放置しないでください。ファームウェアが止まることも、MOSFET が短絡故障することもあります。

## コード

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

#define HEATER_PIN  3        // MOSFET のゲート
#define PWM_CHANNEL 0
#define PWM_FREQ_HZ 1000
#define PWM_BITS    8        // デューティ 0..255

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasHeater       = true,    // ヒーター出力のセル
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "PWM Heater",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);

static void readSensor() {
    const bool ok = s_sht.read();
    s_link.telemetry.airTempC[0]       = ok ? s_sht.getTemperature() : NAN;
    s_link.telemetry.airHumidityPct[0] = ok ? s_sht.getHumidity()    : NAN;
}

static uint32_t s_startMs = 0;

static void stopHeat(uint8_t unit) {
    ledcWrite(PWM_CHANNEL, 0);
    s_link.telemetry.heaterPower01[unit] = 0.0f;
    s_link.status.mode[unit]        = iDryer::UnitMode::Idle;
    s_link.status.targetTempC[unit] = 0.0f;
    s_link.status.durationS[unit]   = 0;
    s_link.status.elapsedS[unit]    = 0;
    s_link.publishStatusNow();
}

static void onHeat(uint8_t unit, JsonObjectConst args) {
    s_link.status.mode[unit]        = iDryer::UnitMode::Heating;
    s_link.status.targetTempC[unit] = args["temperature"].as<float>();
    s_link.status.durationS[unit]   = args["duration"].as<uint32_t>() * 60;   // 0 は制限なし
    s_link.status.elapsedS[unit]    = 0;
    s_startMs = millis();
    s_link.publishStatusNow();
}

static void onStop(uint8_t unit, JsonObjectConst) { stopHeat(unit); }

// 1 秒に 1 回：設定値との差から出力、セッション時間。
static void regulate() {
    if (s_link.status.mode[0] != iDryer::UnitMode::Heating) return;

    const float t = s_link.telemetry.airTempC[0];
    // 測定値がなければヒーターはオフ：見えないまま加熱しない。
    float power = 0.0f;
    if (!isnan(t)) power = constrain((s_link.status.targetTempC[0] - t) * 0.1f, 0.0f, 1.0f);
    ledcWrite(PWM_CHANNEL, (uint32_t)(power * 255));
    s_link.telemetry.heaterPower01[0] = power;

    const uint32_t elapsed = (millis() - s_startMs) / 1000;
    s_link.status.elapsedS[0] = elapsed;
    if (s_link.status.durationS[0] && elapsed >= s_link.status.durationS[0]) stopHeat(0);
}

void setup() {
    ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_BITS);
    ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(8, 9);
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    auto& card = s_link.card();
    card.action("heat", "HEATING", onHeat)
        .name("ru", "Нагрев").name("en", "Heat")
        .param("temperature", "target_temperature", 30, 70, 1, 45, "°C")
        .param("duration", "duration", 0, 720, 10, 120, "min");
    card.action("stop", "IDLE", onStop)
        .name("ru", "Стоп").name("en", "Stop");

    s_link.every(2000, readSensor);
    s_link.every(1000, regulate);
}

void loop() {
    s_link.loop();
}
```

## 仕組み

- `HEATING` モードのアクションはセッションを始めます。コールバックはモード、設定値、時間を設定して `publishStatusNow()` を呼びます。カードはモードからセッションのブロックと **Stop** を表示します。
- purpose の `target_temperature` と `duration` はポータルとアプリが自動でラベル付けします。`args` の数値はすでにパラメーターの範囲に収められています。
- `durationS = 0` は時間制限なしです。`elapsedS` はカードに経過時間として表示されます。
- `hasHeater` で出力のセルが出ます。動作中、コアは直近 2 回の送信周期にわたる `heaterPower01` の平均を送るため、頻繁な PWM の変化でカードの表示が跳ねません。
- ここの制御は比例制御で、設定値より 10 °C 低いと最大出力です。正確に保つには PID に置き換えてください。
- `ledcSetup` / `ledcAttachPin` は arduino-esp32 2.x の API です。3.x では `ledcAttach(pin, freq, bits)` と `ledcWrite(pin, duty)` を使います。

## 確認

カードで **Heat** を開始すると、設定値と時間の入ったセッションのブロックが現れ、出力のセルにヒーターのオンの割合が表示されます。**Stop** で出力が切れます。

## 次へ

- [デバイスカード：card マニフェスト](../09-add-product/02-add-widget.md)：デバイスメニューからのパラメーター、プロファイル、カードのレイアウト。
- [新しい製品の追加方法](../09-add-product/01-add-new-product.md)。
