# MQTT トピックとメッセージ

すべてのトピックは `idryer/{serial}/{suffix}` の形式で、`{serial}` はデバイスシリアル番号です。

このドキュメントは、`idryer-core` の `MqttClient` によって実装されたトピックとコマンドについて説明しています。完全なプラットフォーム インターフェース（すべてのデバイスタイプ向けのすべてのバックエンド コマンド）は `contracts/portal_backend_status.md` にあります。

## デバイス → バックエンド

### info

```
idryer/{serial}/info    retained=true    publish QoS=0
```

デバイスが初めてオンラインになったとき、および `ping` コマンドを受け取ったときに発行されます。

ペイロードはプロダクトが `IProfile::buildInfoJson()` を通じて定義します。バックエンドが最小限で期待するフィールド：`hardwareVersion`、`firmwareVersion`、`timestamp`。

Storage Link の例：

```json
{
  "deviceType": "storage_link",
  "firmwareVersion": "1.0.0",
  "hardwareVersion": "1.0",
  "timestamp": "2026-04-28T10:00:00Z"
}
```

### telemetry

```
idryer/{serial}/telemetry    retained=false    interval ~10 s
```

プロダクトが `pub.publishTelemetry()` を介して発行されます。ライブラリは自動的には発行しません。

Storage Link（気候センサー）の例：

```json
{
  "units": [
    {"unitId": "U1", "temperature": 23.5, "humidity": 47.2}
  ]
}
```

### status

```
idryer/{serial}/status    retained=true    published on change
```

プロダクトが `pub.publishStatus()` を通じて状態変更時に発行されます。ペイロードはプロダクトで定義されます。

### config

```
idryer/{serial}/config    retained=false    on request
```

`device.getConfig`（invoke）の受信時またはイベント `get_config` への応答として発行されます。`pub.publishConfig()` または `pub.publishConfigRaw()` を通じて呼ばれます。

大きなペイロード（> 16000バイト）の場合、チャンク単位で発行：各チャンク含む `tid`、`idx`、`total`、`last`、`d`。

### config/delta

```
idryer/{serial}/config/delta    retained=false    on change
```

`pub.publishConfigDelta()` を通じた部分的な設定更新。バックエンドは `d` フィールド（変更を含むオブジェクト）を期待します。

### events

```
idryer/{serial}/events    retained=false    on event
```

プロダクトが `pub.publishEvent()` を介して発行されます。ライブラリはイベントを自動的に生成しません。

### integrations/status

```
idryer/{serial}/integrations/status    retained=true    on change
```

`LinkIntegrationsManager` によって発行されます。アクティブな統合接続状態を含みます。

### offline (LWT)

```
idryer/{serial}/offline    retained=false    on unexpected disconnect
```

TCP接続がドロップしたときにブローカーによって自動的に設定されます。デバイスは手動でこのトピックを発行しません。

## バックエンド → デバイス

デバイスは `idryer/{serial}/commands/#` にサブスクリプションします。

### commands/ping

```
idryer/{serial}/commands/ping
```

直接 `IdryerRuntime` で処理 — `CommandHandler` に渡されません。

`data["timestamp"]`（形式 `"YYYY-MM-DDTHH:MM:SSZ"`）を抽出し、`settimeofday()` でシステム時刻を同期してから、情報ペイロードを再発行します。

```json
{"timestamp": "2026-04-28T10:00:00Z"}
```

### commands/invoke

```
idryer/{serial}/commands/invoke
```

プロダクト アクションの推奨パス。ライブラリはコマンドをプロダクトの `CommandHandler` に渡します（推奨パス）。`CommandHandler` が登録されていない場合、コマンドは組み込み `ActionDispatcher`（フォールバック）にフォールバックします。

```json
{"action": "led.pulse", "args": {"color": "FF0000", "duration": 500}}
```

組み込みアクション `device.getConfig` はランタイムまたはプロダクト ハンドラーで処理 — `IProfile::getConfig()` を呼びます。結果を発行します。

### commands/set

```
idryer/{serial}/commands/set
```

単一の設定パラメーターを設定します。プロダクトの `CommandHandler`（推奨パス）に渡されます。フォールバック — `CommandHandler` が登録されていない場合、組み込み `ActionDispatcher::handleSet()`。

```json
{"id": 3, "val": 55}
```

### commands/link_integration

```
idryer/{serial}/commands/link_integration
```

統合管理。プロダクトの `CommandHandler` を通じて `LinkIntegrationsManager` で処理されます。

```json
{"type": "bambu", "enabled": true, "ip": "192.168.1.50", "serial": "...", "lanAccessCode": "..."}
```

### commands/bambu_apply

```
idryer/{serial}/commands/bambu_apply
```

フィラメント パラメーターを Bambu プリンターの AMS スロットに適用します。`LinkIntegrationsManager` で処理されます。

### その他のプラットフォーム コマンド

コマンド `drying`、`storage`、`profile`、`stop`、`get_config`、`read_rfid`、`write_rfid` および他は完全な iDryer プラットフォーム インターフェースの一部です。`idryer-core` によって直接処理されていません。プロダクトの `CommandHandler` に配信されます。参照：`contracts/portal_backend_status.md`。

## トピック形式

```c
// Topic construction
idryer_make_topic(buf, sizeof(buf), serialNumber, "telemetry");
// → "idryer/DEVICE_AABBCCDDEEFF/telemetry"
```

サフィックス定数は `mqtt/idryer_topics.h` で定義されています：

```c
#define IDRYER_TOPIC_INFO               "info"
#define IDRYER_TOPIC_TELEMETRY          "telemetry"
#define IDRYER_TOPIC_STATUS             "status"
#define IDRYER_TOPIC_CONFIG             "config"
#define IDRYER_TOPIC_CONFIG_DELTA       "config/delta"
#define IDRYER_TOPIC_EVENTS             "events"
#define IDRYER_TOPIC_OFFLINE            "offline"
#define IDRYER_TOPIC_INTEGRATIONS_STATUS "integrations/status"
#define IDRYER_TOPIC_CMD_WILDCARD       "commands/#"
```
