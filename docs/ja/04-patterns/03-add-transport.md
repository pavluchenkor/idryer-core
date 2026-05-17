# devicePublisherを介した発行

## 使用する場合

`iDryer::Link` には既に 2 つの組み込みトランスポート（MQTT（クラウド）とLocal WebSocket（LAN））が含まれています。ほとんどのタスクでは、追加トランスポートは不要です。

`s_link.devicePublisher()` を使用するのは、プロダクトが独自のペイロードをアセンブルして両方のチャネルに同時に送信する必要がある場合です。例えば、`commands/get_config` への応答でメニュー設定を発行する場合です。

## すぐに使えるコード

```cpp
// main.cpp (fragment)
#include <iDryer.h>

static iDryer::Link s_link(CFG);

// Publish an arbitrary JSON payload to MQTT and Local WS in a single call.
static void publishConfig() {
    static char buf[1024];
    size_t len = buildConfigJson(buf, sizeof(buf));  // product function
    if (len == 0) return;
    s_link.devicePublisher()->publishConfigRaw(buf, len);
}
```

単一の `publishConfigRaw` 呼び出しで、ペイロードは MQTT トピック `idryer/{serial}/config` と全アクティブなLAN WSクライアントに配信されます。追加のクライアントやトピックを作成する必要はありません。

## 説明

`devicePublisher()` はファサードのデュアルパブリッシュヘルパーです。非標準トピックに発行する必要がない限り、`mqttClient()` または `LocalAccess` を直接呼ぶ代わりにこれを使用します。

テレメトリとステータスはファサードのタイマーで自動的に発行されます。`devicePublisher()` はそれらには不要です。

## 第3のトランスポートが必要な場合

3番目のチャネル（BLE、Serial JSON、UARTプロキシ）の追加はレシピパターンではなく、ファサードのアーキテクチャ拡張です。デバイスの圧倒的多数はこれを必要としません。

必要な場合、エントリーポイントは `lib/idryer-core/src/cloud/`（クラウド状態機械、MQTT）と `lib/idryer-core/src/`（ローカルアクセス）内にあります。進める前に、組み込みのMQTTとLocal WSがあなたのユースケースに不十分であることを確認してください。

## リポジトリ内の完全な例

`iDryer-Storage/src/main.cpp:171` の `publishFullMenu()` — `s_link.devicePublisher()->publishConfigRaw(buf, len)` を介した完全なJSONメニューの発行。
