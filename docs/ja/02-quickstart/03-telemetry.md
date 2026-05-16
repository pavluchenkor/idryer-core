# ステップ 03 — テレメトリ: センサー データを発行します

このステップの後、ESP32 は SHT31 センサーから温度と湿度を読み取り、10 秒ごとにポータルに値を発行します。ポータルはそれらをライブ グラフとして表示します。

## 必要なもの

**ハードウェア:**

- SHT31 on I2C ブレークアウト モジュール (アドレス 0x44 または 0x45)
- ワイヤー: SDA、SCL、VCC (3.3 V)、GND

**ソフトウェア:**

- PlatformIO
- ライブラリ `robtillaart/SHT31 @ ^0.5.0`

## 手順

**1. SHT31 を ESP32-C3 に接続します** (Storage Link で使用されるデフォルト ピン):

| SHT31 | ESP32-C3 |
|-------|----------|
| VCC   | 3.3 V    |
| GND   | GND      |
| SDA   | GPIO 8   |
| SCL   | GPIO 9   |

!!! warning
    ボードの電源を切った状態でのみセンサーを接続します。

**2. ライブラリを `platformio.ini` に追加します:**

```ini
lib_deps =
    robtillaart/SHT31 @ ^0.5.0
    ; ... other dependencies
```

**3. `main.cpp` に Wire とセンサーを含めます。** [`iDryer-Storage/src/main.cpp`](../../../../iDryer-Storage/src/main.cpp) に基づいています:

```cpp
#include <Wire.h>
#include "storage/sensors/Sht31ClimateSensor.h"

static Sht31ClimateSensor s_sensor(&Wire);
static bool s_sensorOk = false;
```

**4. `setup()` で初期化します:**

```cpp
Wire.begin(8, 9);  // SDA=8, SCL=9
s_sensorOk = s_sensor.begin();  // auto-detects address 0x44 or 0x45
```

`begin()` がセンサーを見つけない場合、`false` を返します。デバイスはそれなしで実行を続けます。

**5. `loop()` で `tick()` を呼び出し、テレメトリ フィールドを更新します:**

```cpp
if (s_sensorOk) {
    s_sensor.tick(millis());
    SensorReading r = s_sensor.get();
    if (r.ok) {
        s_link.telemetry.airTempC[0]       = r.temperature;
        s_link.telemetry.airHumidityPct[0] = r.humidity;
    }
}
```

ライブラリは、`iDryer::Config` で `telemetryPeriodMs` で設定された間隔で、すべての `telemetry.*` フィールドを MQTT に自動的に発行します。デフォルトは 10 000 ms です。

**6. `iDryer::Config` で機能を有効にします:**

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasAirTemp     = true,
    .hasAirHumidity = true,
    .telemetryPeriodMs = 10000,
};
```

## 検証

Serial Monitor を開きます。センサー検出に成功すると:

```
[MAIN] SHT31 at 0x44
```

ポータルで、デバイス ページに移動 — 温度と湿度の読み取り値は 10 秒ごとに更新されます。

センサーが見つからない場合、警告がログに記録され、デバイスは実行を続けます。アドレス 0x44/0x45 がバス上の別のデバイスに占有されていないことを確認します。

## 次は?

- [04-leds.md](04-leds.md) — センサー データで LED ストリップ カラーを視覚化します。
- [Sht31ClimateSensor.h](../../../../iDryer-Storage/src/storage/sensors/Sht31ClimateSensor.h) — センサー実装。
