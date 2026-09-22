# テレメトリ：カードにセンサーを

このページを終えると、デバイスは SHT31 から温度と湿度を読み、カードにはそのセルと独自の値である露点が表示されます。

## 必要なもの

- SHT31 モジュール（I2C、アドレス 0x44 または 0x45）；
- 配線：SDA、SCL、3.3 V、GND；
- `platformio.ini` に：

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
```

!!! warning
    センサーはボードの電源を切ってから接続してください。

## コード

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,
    .unitsCount      = 1,
    .hasAirTemp      = true,    // 温度のセル
    .hasAirHumidity  = true,    // 湿度のセル
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Climate Sensor",
};
static iDryer::Link s_link(CFG);
static SHT31 s_sht(0x44, &Wire);   // アドレス 0x44 または 0x45（モジュールのジャンパーで決まる）

// マグヌスの式による露点：独自の値の例。
static float dewPointC(float t, float rh) {
    const float g = logf(rh / 100.0f) + 17.62f * t / (243.12f + t);
    return 243.12f * g / (17.62f - g);
}

static void readSensor() {
    if (s_sht.read()) {
        s_link.telemetry.airTempC[0]       = s_sht.getTemperature();
        s_link.telemetry.airHumidityPct[0] = s_sht.getHumidity();
    } else {
        // データなしは NAN：項目は送信されず、カードには「—」。
        s_link.telemetry.airTempC[0]       = NAN;
        s_link.telemetry.airHumidityPct[0] = NAN;
    }
}

void setup() {
    Wire.begin(8, 9);                  // SDA、SCL：お使いのボードのピン
    s_sht.begin();

    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });

    s_link.onTelemetryPublish([](JsonObject root) {
        const float t  = s_link.telemetry.airTempC[0];
        const float rh = s_link.telemetry.airHumidityPct[0];
        if (!isnan(t) && !isnan(rh)) root["units"][0]["dewPointC"] = dewPointC(t, rh);
    });
    s_link.card().sensor("dew_point", "Dew point", "°C", "units[0].dewPointC", "temperature");

    s_link.every(2000, readSensor);    // 2 秒ごとに読み取り
}

void loop() {
    s_link.loop();
}
```

## 仕組み

- `Config` の `hasAirTemp` と `hasAirHumidity` で、カードに温度と湿度のセルが出ます。ポータル側のコードは不要です。
- `s_link.telemetry.*` の項目はコアが自動で送信します。ユニットがひとつでも動作中なら 30 秒ごと、そうでなければ 60 秒ごとです。周期は `Config.telemetryPeriodMs` と `telemetryPeriodIdleMs` で変えられ、0 は契約の値です。
- `NAN` はデータなしで、項目は送信されません。代わりに 0 を入れるとグラフに偽の落ち込みが出ます。
- 独自の値：`onTelemetryPublish` は送信前にテレメトリへ項目を追加し、`card().sensor(id, label, unit, path, deviceClass)` はそれをカードに宣言します。`path` はテレメトリ内のパス、`deviceClass` は任意でアイコンと表示形式を決めます。
- `s_link.every(ms, fn)` は、接続を止めずに `loop()` から指定周期で関数を呼びます。

## 確認

書き込みから 1 分以内に、カードに温度、湿度、露点が表示されます。センサーがなければセルは空のままで、デバイスは動作を続けます。

## 次へ

[LED テープ](04-leds.md)。
