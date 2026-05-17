# トラブルシューティング

`idryer-core` で作業するときの一般的な症状、その原因、解決策。

読む前に、HALログが有効になっていることを確認してください (`idryer::hal::initArduinoHal(&Serial)`) そして `-DCORE_DEBUG_LEVEL=3` 以上が `platformio.ini` に設定されています。

## WiFi

### `WifiConnecting` で状態マシンがハング

症状: ログが `state: WifiConnecting` を繰り返し、`Provisioning` への遷移は発生しません。

考えられる原因:

- 不正なSSID/パスワード。`secrets.h` の `WIFI_SSID` / `WIFI_PASSWORD` をチェックしてください。Improvプロビジョニング後、認証情報はNVSから来ます。`secrets.h` からではありません。
- 5 GHzネットワーク。ESP32は2.4 GHzのみをサポートしています。
- 隠しネットワークまたはルーター上のMACフィルター。
- `WiFi.begin()` が `idryer::hal::initArduinoHal(...)` の前に呼ばれました — ログ出力がありませんが、これはハングの原因ではなく、目の不自由さです。

何をチェックするか:

```cpp
HAL_LOG_INFO("DBG", "WiFi status: %d", WiFi.status());  // 3 = WL_CONNECTED
```

### WiFiが接続するが30～60秒後にドロップ

通常: 信号が弱い (`RSSI < -80 dBm`)、ESP32-C3が専用の5V/1A供給なしでUSBハブから給電されている、FreeRTOSタスクとの競合。

製品ループでRSSIをログ:

```cpp
if (millis() - lastRssi > 30000) { lastRssi = millis(); HAL_LOG_INFO("WIFI", "RSSI: %d dBm", WiFi.RSSI()); }
```

## プロビジョニングと請求

### `Provisioning` で状態マシンがハング

症状: `state: Provisioning` が `Registering` または `AwaitingClaim` に遷移しません。

原因:

- build_flags の `IDRYER_API_BASE` が不正です。`https://portal.idryer.org/api` (本番環境) または `https://staging.idryer.org/api` (ステージング環境) である必要があります。
- TLS証明書がありません (Let's Encrypt ISRG Root X1)。`root_ca.h` に埋め込まれていますが、`MQTT_USE_TLS` なしでビルドされた場合、HTTPクライアントもTLSを使用します — ルートCAはHTTP APIにも必要です。
- デバイス時刻が同期されていません (TLSハンドシェイクには有効な日付が必要です)。`setStateChangeCallback` で `WifiConnecting` の後に `configTime(...)` が呼ばれていることを確認してください (Storage Linkの場合と同じ)。

### `AwaitingClaim` で状態マシンがハング

これはユーザーがポータルにPINを入力していない間の正常な状態です。PINは `setClaimPinCallback` を経由してログに出力されます。

自動請求の場合 (UIのないスタンドアロンデバイス):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

`requestClaim()` の後、バックエンドはユーザーがポータルに入力する必要があるPINを発行します。

### `seedSerialFromMac()` がシリアルを生成したが、ポータルで別のシリアルが入力されました

NVSに保存されたシリアルはMAC生成よりも優先されます。`seedSerialFromMac()` はまだシリアルが存在していない場合にのみNVSに書き込みます。シリアルを変更するには、NVSをクリアしてください:

```cpp
s_credentials.clear();
```

## MQTT

### `MqttConnecting` に入った状態マシンが `Online` に達しない

原因:

- ブローカーに到達不可。本番環境: `mqtt.idryer.org:8883`、ステージング環境: `staging.idryer.org:1884`。
- `MQTT_USE_TLS=1` で正しいルートCAがない — ハンドシェイクが無音で失敗します。
- `setBufferSize(16384)` が適用されていません — `PubSubClient` バッファーはデフォルトで256バイトです。`MqttClient` はすでに16384を設定していますが、`PubSubClient` を直接使用する場合 — 自分でバッファーを設定してください。
- ブローカーで異なるクライアントIDとの永続セッションが「ハング」しています。NVSをクリアして再フラッシュします。

### バックエンドからのコマンドが到着しない

サブスクリプションをチェック — `MqttClient` は `idryer/{serial}/commands/#` にQoS 1でサブスクライブします。サブスクリプションが失敗した場合、ログに表示されます:

```
[MQTT] subscribe failed (3 retries) — disconnecting
```

`setCommandHandler()` が `runtime.begin()` **前に** 呼ばれていることを確認してください — そうでなければ、最初のコマンドバッチが失われるかもしれません。

### `PubSubClient` は正確に60秒間隔で切断

これはキープアライブタイムアウトです。MQTTループが十分な頻度で呼ばれていない可能性があります — `s_runtime.loop()` は長いブロックなしでスピンする必要があります。`loop()` に `delay(>500ms)` と長いネットワークコールがないことを確認してください。

## コマンドとハンドラー

### `commands/invoke` が到着しますが `ActionDispatcher` が呼ばれない

`setCommandHandler()` を登録した場合、**`ActionDispatcher` への組み込みフォールバックは無効です**。`IdryerRuntime` はすべて (`ping` を除く) を `CommandHandler` に渡します。`invoke` コマンドに対して明示的に `s_dispatcher.handleInvoke(data)` を呼ぶ必要があります。

テンプレート:

```cpp
static void handleCommand(const char* cmd, JsonObjectConst data) {
    if (strcmp(cmd, "invoke") == 0) { s_dispatcher.handleInvoke(data); return; }
    if (strcmp(cmd, "set") == 0)    { s_dispatcher.handleSet(data);    return; }
    // ... 製品コマンド ...
}
```

### `commands/set` が受信されますが設定が適用されない

`ActionDispatcher::handleSet` は `id` と `val` を抽出し、登録済みの `SetCallback` に渡します。以下をチェックしてください:

- `dispatcher.setSetCallback(onSetCommand, nullptr)` が `setup()` で呼ばれています。
- `onSetCommand` は実際に `s_profile.applyConfig(id, val)` を呼んでいます。
- `applyConfig` は既知の `id` 値に対して `true` を返します。未知のものに対しては `false` を返し、変更は無視されます。

## テレメトリ

### テレメトリが公開されない

`idryer-core` はテレメトリを自動的に公開しません。製品コードは常にこれを行います。

以下をチェックしてください:

- `pub.publishTelemetry(doc)` (または LocalAccess が使用されていない場合は `s_mqtt.publishTelemetry(doc)`) が実際に `loop()` で呼ばれています。
- レート条件がすべての呼び出しをブロックしていません。一般的な間違い:
  ```cpp
  if (millis() - lastTm > 10000) { /* publish */ }
  ```
  最初のパスで `lastTm == 0` で `millis()` はまだ小さいです — ブランチは実行されません。`>=` を使用して、最初のパスで `lastTm` を初期化してください。
- `s_runtime.isOnline() == true`。MQTTは Online の前に切断されています — 公開は行われません。
- `JsonDocument` サイズはペイロードに十分です。`serializeJson` の後に `doc.overflowed()` をチェックしてください。

### `publishTelemetry` が `false` を返す

原因:

- ブローカーに接続されていません (`MqttClient::isConnected() == false`)。
- バッファが超過しました — `MQTT_BUFFER_SIZE` (16384バイト) より大きいペイロード。大きなデータの場合は `publishConfigRaw` (チャンク付き) を使用するか、ペイロードを削減してください。

### `DevicePublisher::publishTelemetry` がWSクライアントに到達しない

`DevicePublisher` はWSクライアントが接続されていない場合、エラーを返しません — WSパートをスキップするだけです。`s_local.isClientConnected()` をチェックしてください。`false` の場合 — クライアントが認証されていないか、接続されていません。

## NTPとシステム時刻

### デバイス時刻が同期されていない

NTP同期は `setStateChangeCallback` で `WifiConnecting` から最初に抜けた後に開始されます:

```cpp
s_cloud.setStateChangeCallback([](idryer::cloud::CloudState prev,
                                   idryer::cloud::CloudState, void*) {
    if (prev == idryer::cloud::CloudState::WifiConnecting) {
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}, nullptr);
```

このコールバックが登録されていない場合、時刻は自動的に同期されません。ブローカーへのTLSハンドシェイクには有効な時刻が必要です。そうでなければ、証明書は有効期限切れまたは未来のものと見なされます。

代替チャネル: `IdryerRuntime` は `commands/ping` を処理し、`data["timestamp"]` を `settimeofday()` で適用します。バックエンドが1分に1回pingを送信する場合、時刻はNTPなしで更新されます。

### 長時間の稼働後にTLSハンドシェイクが失敗

NTPサーバーに到達できず、デバイスが長時間再起動なしで実行されている場合、時刻がドリフトする可能性があります (特にTCXOなしのESP32-C3)。症状: 複数日の稼働後に突然 `connection failed`。

解決策: `pool.ntp.org` がネットワークから到達可能であることを確認するか、バックエンドから `commands/ping` をより頻繁に受信してください。

### `getIsoTimestamp` が1970年を返す

システム時刻がまだ同期されていません。時刻は最初の成功した `configTime` または `commands/ping` の後に表示されます。それまで、`info`/`telemetry` はプレースホルダー付きで公開されます。

## ArduinoJson

### コンパイルエラー: `StaticJsonDocument` は `ArduinoJson` のメンバーではない

ArduinoJson v7を使用しています。`StaticJsonDocument` 型はv6にのみ存在します。解決策:

- `platformio.ini` でv6をピン留め:
  ```ini
  lib_deps = bblanchon/ArduinoJson @ ^6.21.0
  ```
- または、コードをv7 API (`StaticJsonDocument<N>` の代わりに `JsonDocument`) に移行します。`idryer-core` はv6用に書かれています。

### コンパイルエラー: あいまいなオーバーロードまたは型の不一致

2つのバージョンのArduinoJsonが推移的依存関係を通じて1つのプロジェクトに入る可能性があります。チェック:

```bash
pio pkg list -e my-device | grep -i arduinojson
```

**1つ** のバージョンが必要です。2つある場合 — `lib_deps` で明示的にピン留めし、必要に応じて `lib_ldf_mode = chain+` または `lib_ignore` でピン留めしてください。

### `doc.overflowed()` が serializeJson の後で true

`StaticJsonDocument<N>` のサイズがペイロードに対して小さすぎます。`N` を増やすか、めったに呼ばれないパスに対して `DynamicJsonDocument` を使用してください。

## ローカルWS (LocalAccess)

### アプリがLAN上でデバイスを検出しない

mDNS は `s_local.initMdns(serial)` を経由して **シリアル番号が利用可能になった直後に** 開始される必要があります。以下を確認してください:

- ルーターはマルチキャストをブロックしていません。
- アプリはポート81で `_idryer._tcp` を探しています。
- デバイスシリアル番号がポータルに登録されているものと一致しています。

### WSクライアント接続だが `auth_required` を受信

クライアントからの最初のメッセージは `{"type":"auth","token":"<device_token>"}` である必要があります。トークンが無効な場合、`LocalAccess` は `setTokenRefreshCallback()` を呼びます。製品はそのコールバックで `ICredentialStore` からトークンを再度読み込み、`s_local.updateToken(...)` を呼ぶ必要があります。

## メモリと安定性

### 空きヒープが時間とともに減少

`PubSubClient::loop()` と `WebSocketsServer::loop()` はリークしないはずですが、製品コードをチェックしてください:

- よく呼ばれるパスに対して、ヒープ (`DynamicJsonDocument`) ではなくスタック上に `JsonDocument` (`StaticJsonDocument<N>`) を作成してください。
- ESP32-C3上の製品コード内の `String` はヒープをすばやくフラグメント化します — `char[]` と `snprintf` を使用してください。

### `Stack overflow` または `Guru Meditation`

`s_runtime.loop()` はFreeRTOSタスクを生成しません — すべてはArduinoループで実行されます。スタッククラッシュがある場合は、以下を探してください:

- Arduino ループスタック上の大きなローカル `JsonDocument`/`char[8192]` (デフォルト8 KB)。
- 製品コード内の深い再帰。

Arduino ループスタックを増やします:

```ini
build_flags = -DCONFIG_ARDUINO_LOOP_STACK_SIZE=16384
```

## Improv WiFi (Serial経由のプロビジョニング)

### Improv が認証情報を受け入れない

Improv は認証情報を受け取るまで `Serial` を所有する必要があります:

```cpp
idryer::hal::initArduinoHal(nullptr);   // Improvが Serial を保持している間、ログを /dev/null に送信
// ...
if (WiFi.status() == WL_CONNECTED) {
    idryer::hal::initArduinoHal(&Serial);  // ログ出力を復元
}
```

`HAL_LOG_*` がImprovプロトコルと並行して `Serial` に書き込む場合、Improv はチェックサムで失敗します。

### Improv クライアントがデバイスを認識しない

`setDeviceInfo` で `ChipFamily` をチェック。実際のチップと一致する必要があります: `CF_ESP32_C3`, `CF_ESP32_S3`, `CF_ESP32_S2`, `CF_ESP32`。不一致 — Improvクライアントはリストにデバイスを表示しません。

また、Serial ボーレートが115200であることを確認してください。Improvプロトコルはこれを期待しています。

## 統合診断

### 完全な診断出力 (1 Hz)

メニュー → `DIAGNOSTICS → DIAG LOG` (`menu.diag_en`)。デフォルトで無効です。
デバイスUI、ポータル (`commands/set` with `bind=diag_en`)、
またはREPL (`set diag_en 1`) を経由して有効にします。

有効になると、ブロックは1秒に1回Serialに出力されます:

```
=========== iHeater Link diagnostics ===========
[device]    serial=DEVICE_... online=1 uptime=42s
[wifi]      status=3 ssid=Apart_4 ip=192.168.0.140 rssi=-51
[rmt-out]   mode=DRYING target=70.0°C
[active]    bambu
[bambu]     state=CONNECTED  ip=192.168.0.171 serial=<set> lan=<set>
            gcode_state='RUNNING' tray='PLA' chamber_target=0.0 chamber_temp=0.0
[moonraker] state=DISABLED   ws=ws://192.168.0.171:7125
            vc.available=0 vc.target=0.0 vc.temp=0.0 vc.has_sensor=0
[ha]        state=DISABLED   host=<empty>:1883 user=<empty>
[menu]      bambu_en=1 moon_en=0 ha_en=0 diag_en=1  mat_pla=45 ...
================================================
```

リモート診断に有用: ユーザーが `DIAG LOG` を有効にし、出力をコピー → コネクタ状態、lastError、および実際にRMTに行くものが見えます。

### ANOMALY チャネル (イベントベース)

`diag_en` とは独立して、コネクタとヘルパーは予期しない条件で `[!] ANOMALY` プレフィックス付きの別行を書き込みます:

```
[!] ANOMALY HEATER: unknown tray_type='GFA00' — heater OFF (add mapping or check slicer)
[!] ANOMALY BAMBU: report JSON parse error: ... — raw[124]: ...
[!] ANOMALY BAMBU: report has no 'print' object — raw[42]: {"system":...}
```

`[!]` プレフィックスはログストリーム内の異常を視覚的に強調します。何かが「機能していない」とき、Serialで探す最初のものです。

### 接続喪失時の自動OFF (フェイルセーフ)

アクティブな統合が接続を失った場合 (TCP/WS切断)、コネクタは
ターゲット温度をすぐにリセット:

- **Moonraker** — `WStype_DISCONNECTED` → `chamberTarget=0`, `available=false`
  → `auto_heat::onVirtualChamberUpdate(target=0)` → RMT OFF。
- **Bambu** — 遷移 `Connected → !Connected` → `chamberTarget=0`, `trayType=""`
  → `auto_heat::onBambuPrinterStatusUpdate(...)` → RMT OFF。
- **HA** — フェイルセーフはまだ実装されていません。

このロジックがないと、接続が復元されるまで最後に知られたターゲットで加熱が続きます。

### Bambu: gcode_state フィルター

`auto_heat` は `gcode_state == "RUNNING"` または `"PREPARE"` の場合 **のみ** 加熱します。
他のすべての状態 (`IDLE`, `FINISH`, `FAILED`, `PAUSE`, `INIT`, `OFFLINE`,
`SLICING`, `UNKNOWN`, 空) → OFF。

診断するときは、`[bambu]` 診断行の `gcode_state` に注意してください — `IDLE`/`FINISH` を示す場合、アクティブなトレイが存在するかどうかに関係なく加熱はありません。

### プリンターなしでデバッグするためのテストベンチ

実際のプリンターなしで統合をテストするために、製品リポジトリ
(例: iHeater-link) には `fake_moonraker` / `fake_bambu` のようなスタブユーティリティが
含まれている可能性があり、30秒ごとに値のラプを送信します。
