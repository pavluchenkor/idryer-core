# MqttClient

`MqttClient` はデバイスのMQTTクライアントです。`PubSubClient` をラップし、接続を管理し、受信メッセージをルーティングします。すべてのトピックはデバイスシリアル番号から自動的に形成されます。

## 初期化

```cpp
void MqttClient::begin(const char* serialNumber, const char* token);
```

プロビジョニング成功後に `CloudStateMachine` によって呼ばれます。すぐには接続しません。パラメータを設定し、TLSを設定します。

パラメータ：

- `serialNumber` — デバイス シリアル番号。MQTT クライアント ID とユーザー名として使用されます。
- `token` — デバイス トークン。MQTT パスワードとして使用されます。

フラグ `MQTT_USE_TLS=1` で構築された場合、クライアントは Let's Encrypt ルート CA（`root_ca.h` に埋め込まれている）で `WiFiClientSecure` を設定します。

```cpp
mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
mqttClient_.setBufferSize(MQTT_BUFFER_SIZE); // see "Buffer size" below
mqttClient_.setKeepAlive(60);
```

## バッファサイズ {#buffer-size}

`PubSubClient` はデフォルトで256バイトのバッファを使用します。短いメッセージにのみ十分です。iDryerデバイスの場合はあまりに小さいです：メインの「重い」ペイロードはデバイス構成（メニュー）で、1ショットでトピック `idryer/{serial}/config` に発行されます。

`MqttClient` は、バッファを `MQTT_BUFFER_SIZE` に設定し、大きな設定のチャンクサイズを `MQTT_CONFIG_CHUNK_SIZE` に制限します。両方の定数は `lib/idryer-core/src/mqtt/mqtt_client.h` で定義されています：

```cpp
#define MQTT_BUFFER_SIZE        16384  // PubSubClient buffer
#define MQTT_CONFIG_CHUNK_SIZE  16000  // maximum data in one config chunk
```

それらの関係：

- `MQTT_BUFFER_SIZE`（16384バイト）— **1つのMQTTメッセージ** の上限。このサイズより大きいペイロードを使用する `publish*()` 呼び出しは、送信せずに `PubSubClient` によってドロップされます。
- `MQTT_CONFIG_CHUNK_SIZE`（16000バイト）— 1つの `publishConfigRaw` チャンクの中の `"d"`（データ部分）の最大サイズ。384バイトのマージンはチャンク エンベロープの `{"tid":..,"idx":..,"total":..,"last":..,"d":"..."}` プラス自動的に追加される `timestamp` フィールド用に予約されています。

### なぜ16384なのか

この数字は美的理由ではなく、**予想される最大デバイス ペイロード** から選ばれました。これは設定/メニュー転送です：

- Storage Link および Link/iHeater 設定（メニュ）エスケープ付きJSONとしてシリアル化されます。現在のメニューの完全なスナップショットは約10～14 KBに適合します。
- 16384へのマージンはメニュー成長をカバーしており、チャンクに分割する必要はありません。
- 値は4 KBの倍数です。ESP32でのアロケーションに便利です。

プロダクトがより大きい設定を持つ場合（拡張メニューの多くのアイテムまたはバイナリ値など）、2つのパスが利用可能です：

1. **`MQTT_BUFFER_SIZE` を上げる** — `platformio.ini` の `build_flags` を通じてオーバーライドします：
   ```ini
   build_flags = -DMQTT_BUFFER_SIZE=32768
   ```
   RAMの使用に注意：`PubSubClient` はこのバッファを継続的に保持します。ESP32-C3（～400 KB フリー ヒープ）では32 KBは受け入れられますが、さらに先に進むとリスクが増えます。

2. **`publishConfigRaw(json, length)` を使用する** — ペイロードを `MQTT_CONFIG_CHUNK_SIZE` のチャンクに分割します。バックエンドは `tid` / `idx` / `total` / `last` フィールドによってそれらを再アセンブルします。このパスは、RP2040から任意の長さのUART上で設定が来る場合に推奨されます。

### プロダクト発行に適用

同じ16384バイトの上限は `publishTelemetry`、`publishStatus`、`publishEvent` に適用されます。実際には、テレメトリとイベントははるかに小さい（数百バイト）です。設定発行のみがこの制限に近づきます。プロダクトが定期的に大きなペイロード（測定値配列ダンプなど）を発行する場合、そのサイズを事前に推定するか、自分で分割してください。

## 接続

```cpp
bool MqttClient::connect();
```

実行：

1. 永続セッション付きブローカーへの接続（`clean_session = false`）。永続セッションは必須です。なければ、デバイスがオフラインの間に到着するコマンドが失われます。
2. トピック `idryer/{serial}/offline` の LWT メッセージを設定（QoS 1、保持されない）。
3. `idryer/{serial}/commands/#` にサブスクリプション（QoS 1）。失敗時は最大3回まで試み、その後、切断します。

接続とサブスクリプション が成功した場合は `true` を返します。

## ループ

```cpp
void MqttClient::loop();
```

すべての反復で呼ばれます。切断時に再接続してから、受信メッセージを受け取るために `PubSubClient::loop()` を呼びます。

## 発行

すべての発行メソッドは、ドキュメント内にまだ存在しない場合、`timestamp` フィールド（ISO 8601 UTC）を追加します。

| メソッド | トピック | 保持 |
|--------|-------|----------|
| `publishInfoJson(const char* json)` | `idryer/{serial}/info` | yes |
| `publishTelemetry(JsonDocument&)` | `idryer/{serial}/telemetry` | no |
| `publishStatus(JsonDocument&)` | `idryer/{serial}/status` | yes |
| `publishConfig(JsonDocument&)` | `idryer/{serial}/config` | no |
| `publishEvent(JsonDocument&)` | `idryer/{serial}/events` | no |
| `publishIntegrationsStatus(JsonDocument&)` | `idryer/{serial}/integrations/status` | yes |
| `publishConfigRaw(const char* json, size_t len)` | `idryer/{serial}/config` | no |
| `publishConfigDelta(const char* json, size_t len)` | `idryer/{serial}/config/delta` | no |

`publishConfigRaw` は、サイズが `MQTT_CONFIG_CHUNK_SIZE`（16000バイト）を超える場合、ペイロードを自動的にチャンクに分割します。各チャンクには `tid`、`idx`、`total`、`last`、`d` フィールドが含まれます。

!!! note
    `PubSubClient` は常に QoS 0 で発行します（トピック設定に関係なく）。これはライブラリの制限です。

## コマンドの受信

トピック `idryer/{serial}/commands/{cmd}` の受信メッセージはJSONとして解析され、登録された `CommandCallback` に渡されます：

```cpp
void setCommandCallback(CommandCallback callback);
// CommandCallback = std::function<void(const char* command, JsonObjectConst data)>
```

`{cmd}` 部分はトピックから抽出され、最初の引数として渡されます。`IdryerRuntime` は `begin()` でこのコールバックを登録します。

## ヘルパー メソッド

```cpp
static char* getIsoTimestamp(char* buffer); // buffer >= 32 bytes
static char* generateUuid(char* buffer);    // buffer >= 37 bytes
```

`generateUuid` は `esp_random()` に基づいて UUID v4を生成します。

## 制限

- デバイスごとに1つの `MqttClient` インスタンス（`instance_` を通じたシングルトン）。
- 単一の JSON メッセージの最大サイズ — `MQTT_BUFFER_SIZE`（デフォルト16384バイト）。最も重いデバイス ペイロード（通常はシリアル化された設定（メニュー））のサイズ。より大きい設定の場合、定数を `build_flags` で上げるか、自動チャンク分割で `publishConfigRaw` を使用します。[バッファサイズ](#buffer-size) を参照してください。
- TLS はビルド フラグ `MQTT_USE_TLS` によって有効化されます。
