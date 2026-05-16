# 5分で始める

このページの後、ESP32はフラッシュされ、WiFiに接続し、[portal.idryer.org](https://portal.idryer.org/) にオンラインステータスで表示されます。要件：ESP32-C3（DevKit、Super Mini、または互換）、USBケーブル、VS CodeのPlatformIO。

## 1. secrets.hを準備

[`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) をプロジェクトの `include/secrets.h` にコピーし、WiFi SSIDとパスワード（2.4 GHzのみ）を設定します：

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

`include/secrets.h` を `.gitignore` に追加します。

## 2. platformio.iniを設定

プロジェクトルートに `platformio.ini` を作成します：

```ini
[env:blink-demo]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

lib_deps =
    file://path/to/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

`board` をあなたのボードに変更します。`path/to/idryer-core` をライブラリへの実際のパスに置き換えます。

## 3. 01_blink_statusの例をコピー

[`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) の内容をプロジェクトの `src/main.cpp` にコピーします。例はセンサーや追加の依存関係を必要としません — 最小限の構成ルートのみ。

## 4. フラッシュ

```bash
pio run -e blink-demo -t upload
```

## 5. シリアルモニターを開く

```bash
pio device monitor -b 115200
```

期待されるログシーケンス：

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=
[CLOUD] Connecting to WiFi...
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 1234567 (expires in 600s)
```

ステップ6でポータルにPINを入力した後：

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

デバイスが `PIN: ...` メッセージで停止した場合 — それは予想通りです；ステップ6に進みます。

## 6. ポータルでデバイスを要求

[portal.idryer.org](https://portal.idryer.org/) を開き、**デバイスを追加** に移動し、シリアルモニターのPINを入力します。要求が成功すると、デバイスは `オンライン` に移行し、内蔵LEDは500 ms ごとに点滅します。

詳細な要求フロー：[オンボーディング](02-onboarding.md)。

## 次に行うこと

- センサーを追加 — [04-patterns/01-add-sensor.md](../04-patterns/01-add-sensor.md)
- 周辺機器を追加 — [04-patterns/02-add-peripheral.md](../04-patterns/02-add-peripheral.md)
- 完全なAPI参照 — [03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- 内部的な動作方法 — [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md)
