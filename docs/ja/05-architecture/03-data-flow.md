# データフロー

デバイス内で実行中にデータがどのように移動するかの説明。目的は、`idryer-core` がイベント バスもサービス ロケーターも使用しないことを示すことです：参加者はコンポジション ルート内の明示的なポインターによって接続され、各データ方向は個別の読み取り可能なパスです。

「パーツ間でデータをルーティングする方法」のパターン詳細は [04-patterns/99-data-flow.md](../04-patterns/99-data-flow.md) にあります。

## 主要方向

```
                Backend / app
                     │
                     │ MQTT commands/*
                     ▼
        ┌──────────────────────────────┐
        │  MqttClient                  │
        │  parses topic + payload      │
        └──────────────┬───────────────┘
                       │
                       │ CommandCallback
                       ▼
        ┌──────────────────────────────┐
        │  IdryerRuntime               │
        │  ping → settimeofday + info  │
        │  others → CommandHandler     │
        └──────────────┬───────────────┘
                       │
                       │ commandHandler_(cmd, data)
                       ▼
        ┌──────────────────────────────┐
        │  Product handleCommand()     │
        │  invoke / set / get_config / │
        │  product-specific commands   │
        └──────┬───────────────┬───────┘
               │               │
               ▼               ▼
   ActionDispatcher        IProfile             Sensor / Peripheral TODO:
   handleInvoke / Set      getConfig            (product code)
                           applyConfig
                           buildInfoJson
```

```
       Sensor (product)            Profile / executor
            │                           │
            │ tick() / read             │ updates state
            ▼                           ▼
       ┌───────────────────────────────────────┐
       │  Product Publisher                    │
       │  (StorageTelemetryPublisher, …)       │
       │  builds JsonDocument                  │
       └────────────────┬──────────────────────┘
                        │
                        │ pub.publishX(doc)
                        ▼
       ┌───────────────────────────────────────┐
       │  DevicePublisher (optional)           │
       │  dual-publish helper: MQTT + Local WS │
       └─────────┬─────────────────────┬───────┘
                 │                     │
                 ▼                     ▼
            MqttClient            LocalAccess (WS)
            broker                LAN client
```

## 受信コマンド

1. **MQTT** はトピック `idryer/{serial}/commands/{cmd}` にメッセージを配信します。
2. `MqttClient::handleMessage` はペイロードを JSON として解析し、`CommandCallback` を呼びます。
3. `CommandCallback` は `IdryerRuntime` が `begin()` で登録します。`(command, data)` を受け付けます。ここで `command` は `commands/` の後のサフィックスです。
4. `IdryerRuntime::onMqttCommand`：
   - `command == "ping"` の場合、時刻を同期してinfを発行します。さらに渡されません。
   - `commandHandler_` が登録されている場合、その他をすべてプロダクトに渡します。
   - それ以外の場合、フォールバック組み込みパス：`invoke` → `ActionDispatcher`、`set` → `ActionDispatcher`、`device.getConfig` → `IProfile::getConfig`。

5. **Local WS**（使用されている場合）は `{"type":"command","command":"...","data":{...}}` を受け入れ、エンベロープをアンラップして、MQTT パスに登録された同じ `CommandSink` を呼びます。1つのハンドラー、2つのトランスポート。

## 発信データ

ライブラリは、求められない限り何も発行しません。すべての発信メッセージはプロダクトによって開始されます：

| 何 | 開始者 | APIの方法 |
|------|-------------|--------------|
| `info` | `IdryerRuntime`（オンライン時とピング受信時） | `MqttClient::publishInfoJson` |
| `telemetry` | プロダクトパブリッシャー | `MqttClient::publishTelemetry` または `DevicePublisher::publishTelemetry` |
| `status` | 状態変更時のプロダクトコード | `MqttClient::publishStatus` または `DevicePublisher::publishStatus` |
| `config` | `handleCommand` on `device.getConfig` または `get_config` | `MqttClient::publishConfig` |
| `events` | イベント発生時のプロダクトコード | `MqttClient::publishEvent` |
| `integrations/status` | `LinkIntegrationsManager` | `MqttClient::publishIntegrationsStatus` |
| `offline` | ブローカー自動（LWT） | デバイスはこれを発行しません |

## コンポジション ルートのオブジェクト接続

参加者間の参照は、コンストラクターとセッターを通じて明示的に渡されます。グローバル レジストリーはありません。

```
ArduinoWifiManager     ─┐
ArduinoCredentialStore ─┤
HttpApi (← Http)       ─┼──→ CloudStateMachine ──→ IdryerRuntime ──→ MqttClient
MqttClient             ─┘                              ▲
                                                       │
                                ActionDispatcher ──────┤
                                IProfile         ──────┘

                LocalAccess  ──── (setCommandSink) ────→ same handleCommand
                DevicePublisher (&MqttClient, &LocalAccess)

                Sensor  ──→ Publisher  ──→ DevicePublisher  ──→ MqttClient + LocalAccess
                Executor ←── ActionDispatcher (invoke)  ←── handleCommand
```

各接続は `main.cpp` の1行です。これが「明示的なコンポジション ルート」です。

## このデザインの理由

- **魔法なし**：センサーからクラウドへのデータ移動を理解するために、リーダーは `main.cpp` 内のポインターチェーンを見ます。隠れたデータフローはありません。
- **柔軟性**：プロダクトは `DevicePublisher`（MQTT + WS）の使用、MQTT のみへの発行、または追加ロジック付きの独自パブリッシャーの使用を選択します。
- **テスト可能性**：各ノードは明示的な依存関係を持つ個別のクラスです。ノードは残りのスタックを変更せずにモックで置き換えることができます。

## 意図的に不在のもの

- デバイス内のグローバル イベント バスまたはメッセージ ブローカーはありません。
- 「センサーを持っています。独立してデータを発行します」の自動検出はありません。
- デバイスが「すべてのテレメトリプロバイダーを知っている」型レジストリーはありません。

そのような接続がプロダクトで必要な場合、プロダクトはそれをプロダクトコード内に追加します。ライブラリはそれらを課しません。
