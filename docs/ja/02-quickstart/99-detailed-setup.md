# 詳細設定

近道は [5 分で起動](01-five-minutes.md) です。このページでは環境、ビルドフラグ、ログ、開発モードを扱います。

## プロジェクト内のコア

コアは PlatformIO プロジェクトの `lib/idryer-core` に置きます。コピー、git サブモジュール、または共有クローンへのシンボリックリンクのいずれかです。コアの `library.json` が依存（MQTT、ArduinoJson、WebSockets、Improv）を持ち込みます。プロジェクトの `lib_deps` にはセンサーのライブラリだけを書きます。

コアを使った製品のボード：ESP32-C3（DevKit、Super Mini）、ESP32-S3（XIAO ESP32-S3、Waveshare ESP32-S3 Zero）。

## `platformio.ini`

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

| フラグ | 目的 |
|---|---|
| `IDRYER_API_BASE` | ポータル API のアドレス：アクティベーション、紐付け |
| `MQTT_BROKER`、`MQTT_PORT` | ポータルのブローカー |
| `MQTT_USE_TLS=1` | ブローカーとの安全な接続 |
| `lib_ignore = ESPAsyncTCP` | MQTT クライアントの依存に含まれる ESP8266 用トランスポート。ESP32 ではビルドできない |
| `ARDUINO_USB_MODE`、`ARDUINO_USB_CDC_ON_BOOT` | USB-UART のないボードで Serial を USB 経由に |

文字列マクロには外側と内側の両方に引用符が必要です。

## 送信周期

`Config.telemetryPeriodMs`、`telemetryPeriodIdleMs`、`statusPeriodMs`、`statusPeriodIdleMs` の各項目。0 は契約の値です：

| 何 | 動作中 | 待機中 |
|---|---|---|
| テレメトリ | 30 秒 | 60 秒 |
| ステータス | モード、設定値、時間の変更ですぐ。照合は 60 秒ごと | 照合は 5 分ごと |

「待機中」とは、アクティブなモードのユニットがひとつもない状態です。

## ログ

```bash
pio device monitor -b 115200
```

ネットワークがない間、ログは出ません。ポートを Improv が使っているためです。ログは `[BOOT] WiFi ok, logs enabled` の行で有効になります。その後 `STATUS`、`WIPE_IDENTITY`、`PAIR_TOKEN` コマンドが使えます。[アカウントへの紐付け](02-claim.md) を参照。

## 開発モード：`IDRYER_DEV_REPL`

```ini
[env:my-device-dev]
extends = env:my-device
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

このフラグがあると：

- 電源投入直後からログがポートに出る；
- Improv と `STATUS`、`WIPE_IDENTITY`、`PAIR_TOKEN` コマンドは無効で、ポートの受信行はあなたのコードが読む；
- ネットワークはアプリ（ESPTouch）またはコードから渡す：`begin()` の前の `seedWifiCredentialsIfEmpty()`。[Wi-Fi](01-wifi.md) を参照。

公開するファームウェアはこのフラグなしでビルドします。

## 次へ

- [コアの例](https://github.com/pavluchenkor/idryer-core/tree/main/examples)。
- [新しい製品の追加方法](../09-add-product/01-add-new-product.md)。
- [デバイスカード：card マニフェスト](../09-add-product/02-add-widget.md)。
