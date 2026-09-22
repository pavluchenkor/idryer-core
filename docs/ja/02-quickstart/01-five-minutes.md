---
title: "idryer-core でデバイスを 5 分で起動する"
description: "idryer-core の最初のデバイス：PlatformIO プロジェクト、最小のファームウェア、iDryer アプリでの Wi-Fi 設定とアカウントへの紐付け。"
---

# idryer-core でデバイスを 5 分で起動する

このページを終えると、ESP32 はネットワークにつながり、あなたのアカウントに紐付き、ポータル [portal.idryer.org](https://portal.idryer.org/) と iDryer アプリに表示されます。必要なもの：ESP32-C3 ボード（DevKit、Super Mini または互換品）、USB ケーブル、VS Code の PlatformIO、iDryer アプリの入った電話、2.4 GHz の Wi-Fi ネットワーク。

## 1. PlatformIO プロジェクト

```text
my-device/
├── platformio.ini
├── lib/
│   └── idryer-core/      ← コピー、git サブモジュール、またはシンボリックリンク
└── src/
    └── main.cpp
```

`platformio.ini`：

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; ESPAsyncTCP は espMqttClient の依存に含まれる ESP8266 用トランスポートで、ESP32 ではビルドできません。
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
    ; USB-UART のないボード（ESP32-C3 SuperMini）：Serial は USB 経由。
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

コアのライブラリ（MQTT、ArduinoJson、WebSockets、Improv）はコアの `library.json` から自動で入ります。USB-UART のあるボードでは最後の 2 つのフラグは不要です。`board` はお使いのボードに合わせてください。

## 2. コード

例 [`01_blink_status`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) を `src/main.cpp` にコピーします：

```cpp
#include <Arduino.h>
#include <iDryer.h>

#ifndef LED_PIN
#define LED_PIN 8   // ESP32-C3 SuperMini
#endif

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 独自のデバイス
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "Blink Status",
};
static iDryer::Link s_link(CFG);

// 状態ごとの点滅周期。0 は点滅しない。
static uint32_t blinkPeriodMs() {
    if (s_link.isOnline()) return 1000;
    if (!s_link.isBound()) return 200;
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);

    s_link.begin();
    // 紐付け解除：コアがシークレットを消去し、再び紐付けを待つ。
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();

    // テレメトリの項目はコアが自動で送信する。30 秒ごと（待機中は 60 秒ごと）。
    s_link.telemetry.airTempC[0] = temperatureRead();

    const uint32_t period = blinkPeriodMs();
    digitalWrite(LED_PIN, period ? (millis() / (period / 2)) % 2 : HIGH);
}
```

ファームウェアにはネットワークのパスワードもアカウント情報もありません。ネットワークと紐付けはアプリから受け取ります。

## 3. 書き込みとログ

```bash
pio run -t upload
pio device monitor -b 115200
```

Wi-Fi がない間、ログは出ません。コアがポートを Improv のために確保しているためです。LED は速く点滅します：デバイスはネットワークと紐付けを待っています。

## 4. アプリでの Wi-Fi と紐付け

1. デバイスを使う Wi-Fi ネットワークに電話を接続し、ポータルのアカウントで iDryer アプリにログインします。
2. ホーム画面で **新しいデバイスを接続** をタップすると、**Wi-Fi** ステップが開きます。
3. ネットワーク名を確認し、パスワードを入力して **デバイスを接続** をタップします。アプリは最大 90 秒間設定を送信し、デバイスが参加すると **デバイスが接続されました** と表示されます。**次へ** をタップします。
4. **ペアリング** ステップで **ペアリング** をタップします。アプリはネットワーク上のデバイスを見つけ、ポータルから使い捨ての紐付けトークンを取得してデバイスに渡します。
5. **ペアリングが完了しました** の後、デバイスはポータルとアプリのデバイス一覧に表示されます。

ネットワーク接続後のログ：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

紐付け後：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

## 5. 確認

- デバイスがポータルとアプリでオンライン；
- カードに ESP32 チップの温度；
- LED は 1 秒に 1 回点滅。点灯または消灯のままならポータルとの接続なし。

## うまくいかないとき

- アプリがデバイスの接続を確認できない：パスワードとネットワークが 2.4 GHz であることを確認してください。パスワードが違うと、デバイスは再び設定を待ちます。ネットワークを渡す別の方法は [Wi-Fi](01-wifi.md)；
- **ペアリング** ステップでデバイスが見つからない：電話とデバイスは同じネットワークに必要です。ゲストネットワークはデバイス検出をよくブロックします。詳しくは [アカウントへの紐付け](02-claim.md)；
- ビルドとフラグは [詳細設定](99-detailed-setup.md)。

## 次へ

- [テレメトリ](03-telemetry.md)：センサーと独自の値をカードに。
- [コアの例](https://github.com/pavluchenkor/idryer-core/tree/main/examples)：カードのアクション、独自のセンサーとコントロール。
