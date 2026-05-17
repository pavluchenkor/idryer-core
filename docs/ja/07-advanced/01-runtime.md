# IdryerRuntime

`IdryerRuntime` はトップレベルのデバイス コーディネーターです。`CloudStateMachine`、`ActionDispatcher`、`IProfile`、および `MqttClient` を単一のエントリーポイントに接続します：`begin()` / `loop()`。

## コンストラクタ

```cpp
IdryerRuntime::IdryerRuntime(
    cloud::CloudStateMachine* cloud,
    ActionDispatcher*         dispatcher,
    IProfile*                 profile,
    MqttClient*               mqtt
);
```

4つのパラメーターはすべて必須です。`profile` は `nullptr` である可能性があります（ランタイムはメソッドを呼ぶ前にチェックします）。

## スタートアップ

```cpp
void begin();
```

実行：

1. `MqttClient` の内部 `CommandCallback` を登録します。
2. `cloud->begin()` を呼びます。

`setup()` で一度呼び出します。`setCommandHandler()` の後。

## メインループ

```cpp
void loop();
```

各呼び出し：

1. `cloud->loop()` を呼び出します — 状態機械を進行させます。
2. `profile->loop()` を呼び出します — プロダクト ロジック。
3. オンライン状態への最初の遷移時：
   - `profile->onOnline()` を呼び出します。
   - `profile->buildInfoJson()` を呼び出して結果を `idryer/{serial}/info` に発行（保持）します。
4. オンライン状態喪失時：フラグをリセットして次のオンライン遷移が再度発火するようにします。

## 組み込みハンドリング

### ping

```
commands/ping
```

常にランタイムで処理 — `CommandHandler` に渡されません。

`data["timestamp"]`（形式 `"YYYY-MM-DDTHH:MM:SSZ"`）を抽出し、`settimeofday()` でシステム時刻を同期してから、情報ペイロードを再発行します。

## CommandHandler — 単一の拡張ポイント

```cpp
using CommandHandler = std::function<void(const char* command, JsonObjectConst data)>;
void setCommandHandler(CommandHandler handler);
```

`ping` を除くすべての受信コマンドは登録された `CommandHandler` に向けられます。

これが **唯一の公開的方法** コマンド ハンドリングを拡張するためです。MQTTとローカルWSトランスポートが単一のポイントに収束するように使用されます：

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "get_config") == 0 ||
        (strcmp(cmd, "invoke") == 0 && strcmp(data["action"] | "", "device.getConfig") == 0))
    {
        // Respond to both transports:
        s_pub.publishConfig(doc);
        return;
    }
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // product-specific commands...
}

// in setup():
runtime.setCommandHandler(handleCommand);   // MQTT
local.setCommandSink(handleCommand);        // local WS
```

!!! note "CommandHandler が登録されていない場合"
    ランタイムは組み込みルーティングを使用します：`invoke` → `ActionDispatcher`、`set` → `ActionDispatcher`、`invoke device.getConfig` → 設定を発行します。これがデフォルト動作です。互換性のため保持されています。

## オンライン ステータス

```cpp
bool isOnline() const;
```

`CloudStateMachine` がオンライン状態にある場合は `true` を返します。

## ランタイムが実行しないこと

- テレメトリを発行しません。それはプロダクトの責任です。
- MQTT 再接続を直接管理しません。`CloudStateMachine` がそれを処理します。
- デバイス固有の設定パラメーターについて知りません。
