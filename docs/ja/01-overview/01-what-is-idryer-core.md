# idryer-coreとは

iDryerクラウド用にESP32デバイスを構築している場合、このライブラリはWiFiプロビジョニング（Improv）、クレームプロトコル、MQTTセッション（TLS、再接続、時刻同期）、定期的なテレメトリ/ステータス公開、および受信コマンドルーティングを処理します。通常500行のボイラープレートが`link.begin(); link.loop();`に縮小されます。

## 最小限の例

```cpp
#include <iDryer.h>

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .telemetryPeriodMs = 10000,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};
static iDryer::Link link(CFG);

void setup() { link.begin(); }
void loop()  { link.loop(); link.telemetry.airTempC[0] = sensor.read(); }
```

## ライブラリが実行すること

- WiFi接続とキープアライブ；初期設定用のWeb SerialによるImprovプロビジョニング。
- クレームプロトコル：バックエンド内のデバイス登録、PINによるアカウントクレーミング。
- iDryerブローカーとのMQTTセッション：TLS、永続的なセッション、自動再接続、NTP時刻同期。
- タイマー上のテレメトリ（`Telemetry`）およびステータス（`Status`）の定期的な公開。
- 受信コマンド（`commands/invoke`、`commands/set`、`commands/ping`）の製品ハンドラーへのルーティング。
- ローカルWebSocketサーバー：LANクライアントはクラウドと同じストリームを見ます。
- NVS永続化：WiFi認証情報、デバイストークン、リブート全体のメニュー設定。

## ライブラリが実行しないこと

- 製品ハードウェアは管理しません：ファン、ヒーター、LEDストリップ、センサー。
- 乾燥、保管、または照明のビジネスロジックは含みません。
- 製品固有のメニューパラメータについては認識しません。転送するだけです。
- 製品からのデータなしでテレメトリを公開しません：`loop()`内で`link.telemetry.*`を自分で入力します。

## 次に行くべき場所

- [5分で始める](../02-quickstart/01-five-minutes.md)
- [完全なAPI：iDryer::Link](../03-public-api/01-link-api-reference.md)
