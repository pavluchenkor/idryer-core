# idryer-core の仕組み

idryer-core は ESP32 用のライブラリです。デバイスをポータルとアプリにつなぐすべてを引き受けます：

- Wi-Fi：ネットワークは iDryer アプリ（ESPTouch）または USB 経由の Web ページ（Improv）から届く；
- 使い捨てトークンによるアカウントへの紐付けと紐付け解除；
- 再接続付きの安全な MQTT セッション；
- ローカルネットワークでのアクセス：インターネットがなくてもアプリがデバイスを操作できる；
- テレメトリとステータスの送信、コマンドの配送；
- ファームウェアの無線アップデート；
- ポータルとアプリのデバイスカード。

あなたが書くのは自分の部分だけです。センサーを読み、負荷を制御し、カードに何を表示し、どの操作を実行するかを宣言します。

## 入口はひとつ：`iDryer::Link`

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType      = iDryer::DeviceType::Unknown,   // 独自のデバイス
    .unitsCount      = 1,
    .hasAirTemp      = true,                          // 温度のセル
    .hardwareVersion = "1.0",
    .firmwareVersion = "0.1.0",
    .model           = "My Device",
};
static iDryer::Link s_link(CFG);

void setup() {
    s_link.begin();
    s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
}

void loop() {
    s_link.loop();
    s_link.telemetry.airTempC[0] = readTemperature();   // あなたのコード
}
```

| あなた | コア |
|---|---|
| `s_link.telemetry.*` を埋める | 30 秒ごと、待機中は 60 秒ごとに送信 |
| `s_link.status.*`（モード、設定値、時間）を変えて `publishStatusNow()` を呼ぶ | ステータスを届ける。カードはモードで切り替わる |
| `s_link.card()` を宣言：センサー、コントロール、アクション | card マニフェストを組み立てて送信 |
| `s_link.onCommand(...)` を登録 | クラウドとローカルネットワークのコマンドを渡す |

## デバイスカード

ポータルとアプリは、デバイスが送る card マニフェストからカードを描きます：

- `Config.has*` フラグで既製のセルが出る：温度、湿度、ヒーター出力、ファンなど；
- `card().sensor(...)` はテレメトリ内のパスで独自の値を追加する；
- `card().action(...)` は操作を宣言する：実行後のユニットのモードと起動パラメーター。

そのためのポータル側のコードは不要です。詳しくは [デバイスカード：card マニフェスト](../09-add-product/02-add-widget.md)。

## `mqtt_contract.yaml` が唯一の正

[`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) はプロトコルを記述します：トピック、テレメトリとステータスの項目、デバイスの機能、card マニフェスト。ここから生成されるもの：

| 何 | どこへ |
|---|---|
| `iDryer::Config`（`has*` フラグ）と API 構造体 | `src/_generated/iDryer_api.h` |
| MQTT トピック | `contracts/_generated/mqtt_topics.h` |
| ESP32 ↔ コントローラーの UART プロトコル | `contracts/_generated/uart_protocol.h` |
| ポータル用の TypeScript 型 | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    `_generated/` のファイルを手で編集しないでください。`contracts/regen.sh` が契約から上書きします。

独自の値に契約の変更は不要です。`card().sensor(...)` で宣言します。契約を変えるのは、新しい機能がすべての製品に必要なときです：まず `mqtt_contract.yaml`、次に `regen.sh`、それからコード。

## コアを使った製品

- **iDryer Link**：ドライヤーの通信モジュール。コントローラーの隣の ESP32 で、UART でやり取りする。
- **iDryer Storage**：スプールラックの照明。アドレス指定可能な LED テープと SHT31 センサー。
- **iHeater Link**：iHeater ヒーターの制御。Bambu Lab、Klipper/Moonraker、Home Assistant と連携。

## 次へ

[5 分で起動](01-five-minutes.md)。
