# センサーを追加する

## 使用する場合

デバイスが物理センサー（温度、湿度、重さなど）を定期的に読み取り、クラウドまたはLANクライアントに読み値を発行する必要がある場合は、このレシピを使用します。

## すぐに使えるコード

プロジェクトにコピーして、`MyClimate`を自分のクラス名に置き換えます：

```cpp
// MyClimate.h — product sensor driver
#pragma once
#include <stdint.h>

class MyClimate {
public:
    bool  begin();
    void  tick(uint32_t nowMs);  // non-blocking, no delay()
    float temperature() const;
    float humidity()    const;
    bool  ok()          const;
};
```

```cpp
// main.cpp
#include <iDryer.h>
#include "MyClimate.h"

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link  s_link(CFG);
static MyClimate     s_climate;
static bool          s_sensorOk = false;

void setup() {
    s_sensorOk = s_climate.begin();
    s_link.begin();
}

void loop() {
    s_link.loop();

    if (s_sensorOk) {
        s_climate.tick(millis());
        if (s_climate.ok()) {
            s_link.telemetry.airTempC[0]       = s_climate.temperature();
            s_link.telemetry.airHumidityPct[0] = s_climate.humidity();
        }
    }
    // Publishing is automatic, on the telemetryPeriodMs timer from Config.
}
```

## 説明

プロダクトは `loop()` 内で `s_link.telemetry.*` フィールドのみを埋めます。ファサードは `Config.telemetryPeriodMs` ミリ秒ごとに自動的にMQTTとLocal WSに発行します。手動MQTT発行と異なり、`publishTelemetryNow()` を手動で呼ぶ必要はありません。これが手動MQTTとの重要な違いです：`StaticJsonDocument` も `publishTelemetry` も個別のパブリッシャークラスも不要です。

タイマー外で読み値を即座に発行する必要がある場合は、`s_link.publishTelemetryNow()` を呼びます。

`Config` の `hasAirTemp` / `hasAirHumidity` フラグは、JSONにどのフィールドが表示されるかを制御します。フラグが `false` のフィールドは発行されません。

テレメトリフィールドの完全なリスト：[テレメトリフィールド](../03-public-api/01-link-api-reference.md#telemetry-fields)。

## リポジトリ内の完全な例

リファレンス実装：`Sht31ClimateSensor` + `iDryer-Storage/src/main.cpp` の `s_link.telemetry.airTempC[0]` / `s_link.telemetry.airHumidityPct[0]` の埋め込み。
