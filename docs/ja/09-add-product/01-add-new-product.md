---
title: "idryer-coreベースの新しい製品を追加する"
description: "iDryer::Linkファサード上の新しいiDryerデバイスのチェックリスト：プロジェクト、最小限のファームウェア、アプリでのWi-Fiとペアリング、テレメトリ、設定メニュー、デバイスカード、コントラクト。"
---

# idryer-coreベースの新しい製品を追加する

このガイドは、`idryer-core`上で新しい製品（フィラメント乾燥機、加熱ブロック、照明、センサー、その他のモジュール）を作るときに使います。コアが代わりに行うことと、製品コードが追加すべきことを示します。

ビルドできる完全な例は、Build-Your-Own-iDryerドキュメントの加温式保管キャビネット（`example/09-cabinet`）です。このページの内容をすべて通しています。

---

## 製品の土台

製品は1つのオブジェクト — `iDryer::Link`ファサード（`<iDryer.h>`）を通じてコアとやり取りします。`s_link.begin()`と`s_link.loop()`の中で、コアは次を行います：

- iDryerアプリから無線（ESPTouch）で、またはWebインストーラーからUSB（Improv）でWi-Fiネットワークを受け取り、接続を維持する；
- デバイスをアカウントに紐付ける：使い捨てのペアリングトークンを、アプリからローカルネットワーク経由、またはシリアルポート経由（`PAIR_TOKEN:<token>`）で待ち、ポータルで永続的なシークレットと交換する；
- MQTTに接続し、`Config`の周期で`telemetry`と`status`を公開する；
- ローカルネットワークで自分を告知し（mDNS `_idryer._tcp`）、WebSocketでアプリからのコマンドを受け付ける；
- デバイスカードのcardマニフェストを公開する。

下位のクラス（`IdryerRuntime`、`CloudStateMachine`、`LocalAccess`など）はコアの内部です。ペアリングトークンがクラウド部分に届くのは`iDryer::Link`の中だけです。製品はファサードの上に作ってください。

---

## 1. プロジェクト

`idryer-core`は`lib/idryer-core/`に置きます（コピーまたはシンボリックリンク）。PlatformIOはコアのライブラリをその`library.json`から取得します。最小限の`platformio.ini`：

```ini
[env:my-device]
platform    = espressif32
framework   = arduino
board       = esp32-c3-devkitm-1

; espMqttClient の依存に含まれる ESP8266 用トランスポート：ESP32 ではビルドできない
lib_ignore = ESPAsyncTCP

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

`lib_ignore = ESPAsyncTCP`がないとビルドは`ESPAsyncTCP.cpp`で失敗し、`MQTT_BROKER`と`MQTT_PORT`がないとコアはコンパイルできません。

---

## 2. 最小限のファームウェア

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // iDryer製品ではない：カードはマニフェストから作られる
    .unitsCount      = 1,
    .hasAirTemp      = true,
    .hasAirHumidity  = true,
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    // ポータルで紐付けが解除された：シークレットを消去し、新しいペアリングを待つ
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0]       = readTemp();       // あなたのセンサー関数
    s_link.telemetry.airHumidityPct[0] = readHumidity();
}
```

`s_link.begin()`がWi-Fi、紐付け、MQTT、ローカルアクセスを開始します。`s_link.loop()`は`delay()`なしで常に回し続ける必要があります。`revoke`コマンドは、デバイスがアカウントから紐付け解除されたときにポータルから届きます。`handleRevoke()`がシークレットを消去し、デバイスは新しいペアリングトークンを待ちます。

---

## 3. Wi-Fiと紐付け — コードには書かない

ファームウェアにはネットワークのパスワードもアカウント情報も入っていません。ユーザーはiDryerアプリでデバイスを接続します：**新しいデバイスを接続** → **Wi-Fi**ステップ（アプリがESPTouchでネットワークを送信）→ **ペアリング**ステップ（アプリがmDNSでデバイスを見つけ、ポータルから使い捨てトークンを取得してデバイスに渡す。トークンのポータルでの有効化はデバイス自身が行う）。Wi-Fiがつながるまで、シリアルポートには何も出ません。コアがWebインストーラー（Improv）のために空けておくからです。

手順と期待されるログ — Build-Your-Own-iDryer、章「コアでのファームウェアの開始」。

---

## 4. データ：テレメトリとステータス

- `Config`の`has*`フラグは、どの語彙フィールドがテレメトリに入り、どのセルがカードに表示されるかを決めます。
- 値は`s_link.telemetry`（`airTempC`、`airHumidityPct`、`heaterTempC`、`heaterPower01`、`fanOn`、`servoOpen`）と`s_link.status`（`mode`、`targetTempC`、`durationS`、`elapsedS`）に書き込みます。コアは`Config`の周期でそれらを公開し、`s_link.publishStatusNow()`はステータスをすぐに送ります。
- 独自のフィールドは`s_link.onTelemetryPublish()`で、カードには`s_link.card().sensor()`で — [デバイスカード](02-add-widget.md)を参照。

---

## 5. 設定：メニュー

設定は`src/menu/menu.yaml`に記述します。ジェネレーター`menu_gen.py`がそこからC++コード、NVSへの保存、メニューのJSONを作ります（[プロトコルとしてのメニュー](../08-contracts/02-menu-as-protocol.md)）。製品コードは：

- `s_link.begin()`の前にメニューを読み込む：`menu.initDefaults()`、`menu.loadFromNVS()`、`menu_sync_state_to_cache()`；
- `menu_buildFullJson()`と`s_link.devicePublisher()->publishConfigRaw()`で公開する — オンラインになったときと`get_config`コマンドを受けたとき；
- `set`コマンドを`menu_apply_by_bind()`で適用し（値、NVS、キャッシュを一度に）、メニューを再公開する。

完全なコード — Build-Your-Own-iDryer、章「YAMLからのメニュー」。

---

## 6. デバイスカード

カードに何を表示し、どの操作を開始するかは`s_link.card()`で宣言します — [デバイスカード: card マニフェスト](02-add-widget.md)。

---

## 7. コントラクト

新しいトピックを追加したりペイロードを変更したりするとき：

1. `contracts/mqtt_contract.yaml`を更新する；
2. `contracts/regen.sh`を実行し、生成されたファイルをコミットする。

---

## 2チップのデバイス

UARTで別のコントローラー（例：RP2040）と連携するESP32向けに、コアにはUARTブリッジ`idryer_uart.h`があります。動作する参考実装は`idryer-link`ファームウェアです。
