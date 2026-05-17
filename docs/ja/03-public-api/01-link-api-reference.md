# Public API: iDryer::Link

`iDryer::Link` は組込み開発者のための単一の エントリ ポイントです。ファサードはスタック全体を隠しています: WiFi/Improv、クラウド ステート マシン、HTTP クレーム、MQTT、ローカル WebSocket、NVS。プロダクトは `telemetry`/`status` フィールドを埋め、コールバックを登録し、`begin()`/`loop()` を呼び出すだけで済みます。

---

## ライフサイクル

典型的な `main.cpp` スケルトン:

```cpp
#include <iDryer.h>
#include <runtime/idryer_runtime.h>  // only needed if setCommandHandler is used

static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::StorageLink,
    .unitsCount        = 1,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .hasHeaterPower    = false,
    .hasFanStatus      = false,
    .hasScales         = false,
    .hasRfid           = false,
    .allowHa           = false,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 10000,
    .statusPeriodMs    = 0,
    .hardwareVersion   = "1.0",
    .firmwareVersion   = "1.0.0",
};

static iDryer::Link link(CFG);

void setup() {
    link.begin();
    // setCommandHandler — strictly AFTER begin(): begin() installs its own dispatcher
    link.runtime()->setCommandHandler(handleCommand);
}

void loop() {
    link.loop();
    link.telemetry.airTempC[0]       = sensor.readTemp();
    link.telemetry.airHumidityPct[0] = sensor.readHumidity();
}
```

---

## 設定: `iDryer::Config`

`main.cpp` で 1 回埋められ、`Link` コンストラクターに渡されます。すべてのフィールドは集約初期化 (C++ 指定イニシャライザー) を使用します。

| フィールド | タイプ | 目的 | 注記 |
|-------|------|---------|------|
| `deviceType` | `DeviceType` | デバイス タイプ | **必須** |
| `unitsCount` | `uint8_t` | ユニット (チャンバー) の数、1..`MAX_UNITS` (4) | **必須** |
| `hasAirTemp` | `bool` | 気温センサーが存在 | false = JSON から省略 |
| `hasAirHumidity` | `bool` | 湿度センサーが存在 | false = JSON から省略 |
| `hasHeaterTemp` | `bool` | ヒーター温度センサーが存在 | — |
| `hasHeaterPower` | `bool` | ヒーター電力センサーが存在 | — |
| `hasFanStatus` | `bool` | ファン ステータスが存在 | — |
| `hasScales` | `bool` | スケールが存在 | — |
| `hasRfid` | `bool` | RFID リーダーが存在 | — |
| `allowHa` | `bool` | Home Assistant インテグレーションを許可 | false = SDK はクライアントを作成しません |
| `allowBambu` | `bool` | Bambu Lab LAN インテグレーションを許可 | — |
| `allowMoonraker` | `bool` | Moonraker/Klipper インテグレーションを許可 | — |
| `telemetryPeriodMs` | `uint32_t` | `Telemetry` (ms) の自動発行期間 | 0 = 発行しない |
| `statusPeriodMs` | `uint32_t` | `Status` (ms) の自動発行期間 | 0 = 発行しない |
| `hardwareVersion` | `const char*` | ハードウェア バージョン文字列 | **必須** |
| `firmwareVersion` | `const char*` | ファームウェア バージョン文字列 | **必須** |

---

## クラス `iDryer::Link`

### コンストラクター

```cpp
explicit Link(const Config& cfg);
```

const リファレンスで設定を取得します。`CFG` はフル オブジェクト ライフタイム (通常 `static const`) に対して存在する必要があります。

### メソッド

#### `begin()`

```cpp
bool begin();
```

SDK スタック全体を起動します: WiFi/Improv、クラウド ステート マシン、HTTP クレーム、MQTT、ローカル WebSocket、NVS 永続性。

`setup()` で 1 回呼び出します。初期化が成功すると `true` を返します。

```cpp
void setup() {
    link.begin();
}
```

#### `loop()`

```cpp
void loop();
```

唯一の必須ティック。WiFi/MQTT/LocalAccess をサービスし、自動発行テレメトリーとステータスをそれらのタイマーで提供します。

`loop()` の毎回の反復で呼び出します。この呼び出しなしでは接続は維持されません。

```cpp
void loop() {
    link.loop();  // first in loop(), before product logic
}
```

*ソース: `iDryer-Storage/src/main.cpp:253`, `iHeater-link/src/main.cpp:381`.*

#### `publishTelemetryNow()`

```cpp
void publishTelemetryNow();
```

`telemetryPeriodMs` タイマーに関係なく、現在の `link.telemetry` 状態を直ちに発行します。

#### `publishStatusNow()`

```cpp
void publishStatusNow();
```

現在の `link.status` 状態を直ちに発行します。コマンドを処理した後に使用し、新しい状態をポータルに直ちに反映させます。

```cpp
// iHeater-link/src/main.cpp:238
device().publishStatusNow();
```

#### `raiseEvent()`

```cpp
void raiseEvent(EventKind   severity,
                const char* event,
                const char* message,
                uint8_t     unitId = 0xFF);
```

トピック `idryer/{serial}/events` にイベントを発行します。直ちに送信されます。

| パラメーター | タイプ | 目的 |
|-----------|------|---------|
| `severity` | `EventKind` | `Info` / `Warning` / `Error` |
| `event` | `const char*` | イベント コード、例えば `"OVERHEAT"`、`"SESSION_COMPLETE"` |
| `message` | `const char*` | 任意のデバッグ テキスト |
| `unitId` | `uint8_t` | ユニット インデックス (0..unitsCount-1) またはデバイス全体の `0xFF` |

```cpp
link.raiseEvent(iDryer::EventKind::Error, "OVERHEAT", "U1 too hot", 0);
```

#### `onRequest()`

```cpp
void onRequest(RequestCallback cb);
```

MQTT またはローカル WS を経由して到達するビジネス コマンド (`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`) のコールバックを登録します。コマンド ソースは透過的です。

`RequestCallback` = `std::function<void(const iDryer::Request&)>`

```cpp
link.onRequest([](const iDryer::Request& r) {
    switch (r.kind) {
        case iDryer::RequestKind::Start: myStart(r.unitId, r.targetTempC); break;
        case iDryer::RequestKind::Stop:  myStop(r.unitId);                 break;
        default: break;
    }
});
```

**重要:** `runtime()->setCommandHandler(...)` が設定されている場合、このコールバックは呼び出されません — 完全なディスパッチャーはすべてのコマンドをインターセプトします。

#### `onProfile()`

```cpp
void onProfile(ProfileCallback cb);
```

`commands/profile` — マルチステップ 乾燥スケジュール用のコールバックを登録します。

`ProfileCallback` = `std::function<void(const iDryer::ProfileSchedule&)>`

#### `onIntegrationStatus()`

```cpp
void onIntegrationStatus(IntegrationStatusCallback cb);
```

インテグレーション接続状態が変わるとき呼び出されます (HA、Bambu、Moonraker)。オプション コールバック。

`IntegrationStatusCallback` = `std::function<void(const iDryer::IntegrationStatus&)>`

#### `onClaimPin()`

```cpp
void onClaimPin(ClaimPinCallback cb);
```

クラウド クレーム フローがポータルで入力する PIN を返すとき呼び出されます。

`ClaimPinCallback` = `std::function<void(const char* pin, uint32_t expiresInSeconds)>`

```cpp
// iHeater-link/src/main.cpp:367
device().onClaimPin([](const char* pin, uint32_t expiresInSeconds) {
    Serial.printf("CLAIM_PIN:%s:%u\n", pin, expiresInSeconds);
});
```

#### `isOnline()`

```cpp
bool isOnline() const;
```

デバイスが登録されており、MQTT セッションがアクティブな場合、`true` を返します。

```cpp
// iHeater-link/src/main.cpp:281
if (device().isOnline()) { ... }
```

#### `serial()`

```cpp
const char* serial() const;
```

デバイス シリアル番号 (NVS からの文字列、クレーム中に割り当て)。クレームが完了する前は空の文字列。

#### `seedWifiCredentialsIfEmpty()`

```cpp
void seedWifiCredentialsIfEmpty(const char* ssid, const char* password);
```

WiFi 認証情報がまだ設定されていない場合のみ、NVS に書き込みます。`begin()` の前に呼び出します。ハードコードされた認証情報を持つ dev 環境で使用されます。

#### `setWifiCredentials()`

```cpp
void setWifiCredentials(const char* ssid, const char* password);
```

常に NVS の WiFi 認証情報を上書きします。Dev ヘルパーと強制的な再プロビジョニング。

```cpp
// iHeater-link/src/main.cpp:313
device().setWifiCredentials(ssid.c_str(), pass.c_str());
```

#### `requestClaim()`

```cpp
bool requestClaim();
```

クラウド クレーム フローを手動で開始 (プロビジョニング → 登録 → チェック-クレーム)。成功すると登録された `onClaimPin` コールバックを呼び出します。リクエストが受け入れられた場合 `true` を返します。

```cpp
// iHeater-link/src/main.cpp:284
bool ok = device().requestClaim();
```

#### `eraseClaimAndRestart()`

```cpp
void eraseClaimAndRestart();
```

NVS からデバイス トークンを削除し、チップを再起動します。再起動後、デバイスはクレームされていません — 自動クレーム フローが再度開始されます。この関数は戻りません。

```cpp
// iHeater-link/src/main.cpp:293
device().eraseClaimAndRestart();
```

#### `integrationsManager()`

```cpp
idryer::cloud::LinkIntegrationsManager* integrationsManager();
```

インテグレーション マネージャーへのアウトレット — プロダクト側のワイヤリング用 (Moonraker チャンバー ターゲット コールバック、Bambu プリンター ステータスなど)。

`#include <integrations/common/link_integrations_manager.h>` が必要です。

```cpp
// iHeater-link/src/main.cpp:337
device().integrationsManager()->setVirtualChamberCallback(onVirtualChamberUpdate);
```

#### `mqttClient()`

```cpp
idryer::MqttClient* mqttClient();
```

SDK MQTT クライアントへのアウトレット — 独自のトピックを発行するかコマンド ルーティング (例えば `MenuBridge`) に統合するコンポーネント用。

`#include <mqtt/mqtt_client.h>` が必要です。

#### `devicePublisher()`

```cpp
idryer::DevicePublisher* devicePublisher();
```

デュアル発行ヘルパーへのアウトレット — 1 つのペイロードを MQTT とローカル WS の両方に同時に送信します。自動発行テレメトリーと同じ方法でポータルに到達する必要があるプロダクト応答に使用します。

```cpp
// iDryer-Storage/src/main.cpp:175
link.devicePublisher()->publishConfigRaw(buf, len);
```

#### `runtime()`

```cpp
idryer::IdryerRuntime* runtime();
```

SDK ランタイムへのアウトレット — ファサード ディスパッチャーの代わりに完全なコマンド ハンドラーを設定するために使用。`setCommandHandler(...)` 後、ファサードの `onRequest`/`onProfile` は MQTT パス経由では呼び出されません。

**重要:** `begin()` の後に厳密に呼び出します — `begin()` は独自のディスパッチャーをインストールします。これは上書きされる必要があります。

```cpp
// iDryer-Storage/src/main.cpp:249
link.runtime()->setCommandHandler(handleCommand);

// Handler signature:
// void handleCommand(const char* cmd, JsonObjectConst data);
```

`#include <runtime/idryer_runtime.h>` が必要です。

---

### テレメトリー フィールド {#telemetry-fields}

`loop()` でプロダクトによって埋められます。SDK は `telemetryPeriodMs` タイマーで読み取り、MQTT とローカル WS に発行します。

| フィールド | タイプ | 設定フラグ | 目的 |
|-------|------|-------------|---------|
| `telemetry.airTempC[unitId]` | `float` | `hasAirTemp` | 気温、°C |
| `telemetry.airHumidityPct[unitId]` | `float` | `hasAirHumidity` | 湿度、% |
| `telemetry.heaterTempC[unitId]` | `float` | `hasHeaterTemp` | ヒーター温度、°C |
| `telemetry.heaterPower01[unitId]` | `float` | `hasHeaterPower` | ヒーター電力、0.0..1.0 |
| `telemetry.fanOn[unitId]` | `bool` | `hasFanStatus` | ファン ステータス |
| `telemetry.weightG[unitId]` | `uint16_t` | `hasScales` | 重量、グラム |

```cpp
// iDryer-Storage/src/main.cpp:267
link.telemetry.airTempC[0]       = r.temperature;
link.telemetry.airHumidityPct[0] = r.humidity;
```

`unitId` = 最初の (または唯一の) ユニットの場合 0。インデックスは < `Config.unitsCount` である必要があります。

`Status` フィールド — 同じ構造ですが、操作状態:

| フィールド | タイプ | 目的 |
|-------|------|---------|
| `status.mode[unitId]` | `UnitMode` | 現在のユニット モード |
| `status.targetTempC[unitId]` | `float` | 目標温度 |
| `status.durationS[unitId]` | `uint32_t` | リクエストされた期間、s (0 = 無制限) |
| `status.elapsedS[unitId]` | `uint32_t` | セッション開始からの経過時間、s |

```cpp
// iHeater-link/src/main.cpp:229
device().status.mode[0]        = iDryer::UnitMode::Drying;
device().status.targetTempC[0] = cmd.targetTempC;
device().publishStatusNow();
```

### ランタイム経由のコールバック登録

着信コマンドの完全な制御が必要な場合 (例えば、プロダクトが `get_config`、`set`、非標準 `invoke` を処理):

```cpp
// Signature — from idryer_runtime.h
void handleCommand(const char* cmd, JsonObjectConst data);

// Registration — strictly after link.begin()
link.runtime()->setCommandHandler(handleCommand);
```

`cmd` — コマンド文字列 (`"set"`、`"invoke"`、`"ping"`、`"get_config"`)。
`data` — ArduinoJson `JsonObjectConst` with ペイロード。

このアプローチでは、`onRequest()` と `onProfile()` は MQTT パスから呼び出されません — プロダクトはコマンドを直接処理します。

---

## 列挙

### `iDryer::DeviceType`

| 値 | 数値 | 目的 |
|-----|---------|---------|
| `Unknown` | 0 | なし / 未定義 |
| `Dryer` | 1 | ドライヤー (iDryer LINK) |
| `Heater` | 2 | ヒーター |
| `StorageLink` | 4 | ストレージ リンク (ESP32-C3 + LED) |
| `IHeaterLink` | 5 | iHeater リンク |

### `iDryer::UnitMode`

`Idle`、`Drying`、`Storage`、`Profile`、`Fault`、`Unknown`

### `iDryer::EventKind`

`Info`、`Warning`、`Error`

### `iDryer::RequestKind`

`Start`、`Stop`、`Storage`、`Find`、`ClearErrors`

### `iDryer::IntegrationKind`

`Ha`、`Bambu`、`Moonraker`

### `iDryer::IntegrationState`

`Disabled`、`Idle`、`Connecting`、`Online`、`ConfigMissing`、`Error`

---

## より深く掘る場合

ファサードはほとんどのタスクに十分です。`idryer::IdryerRuntime`、`idryer::MqttClient`、`idryer::cloud::LinkIntegrationsManager` でファサード レベルより下で作業する必要がある場合 — アーキテクチャ セクションを参照してください。
