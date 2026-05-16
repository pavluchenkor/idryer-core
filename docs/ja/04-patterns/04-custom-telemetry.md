# カスタムテレメトリ（プロダクト固有のペイロード）

## 使用する場合

idryer-core の標準テレメトリは、共通コントラクトで定義されたフィールドのみを発行します（`units[].temperature`、`humidity`、`heaterPower` など）。プロダクトがトップレベルの JSON フィールド（`outputMode`、`targetTempC`、`active` など）を追加するか、`Telemetry` 構造体に存在しないデータを含める必要がある場合は、このレシピを使用します。

典型的なケース：iHeater Link は標準の `units[]` と共に `outputMode` と `targetTempC` を発行するため、バックエンドは `telemetry:update` WebSocket イベント経由で `heaterIntent` をフロントエンドに転送できます。

## ステップ 1 — 自動発行を無効化する

`Config` の `telemetryPeriodMs = 0` を設定します。これにより、idryer-core が削減されたペイロードを単独で発行するのを防ぎます：

```cpp
static const iDryer::Config CFG = {
    // ...
    .telemetryPeriodMs = 0,   // publish manually
    .statusPeriodMs    = 5000,
};
```

## ステップ 2 — 発行関数を記述する

`device().mqttClient()->publishTelemetry(doc)` を使用します。バックエンドが期待するすべてのフィールドを含めます：プロダクト固有のもの（トップレベル）と標準的な `units[]` ブロック。

```cpp
#include <integrations/common/link_integrations_types.h>  // activeIntegrationToString()

static void publishCustomTelemetry() {
    auto* mqtt = device().mqttClient();
    if (!mqtt) return;

    // Current hardware output intent
    const auto cmd     = s_output.getLastCommand();
    const bool heating = (cmd.mode == ControllerOutputMode::TargetTemperature);

    // Active integration ('bambu' / 'moonraker' / 'ha' / 'none')
    using AI = idryer::cloud::ActiveIntegration;
    const AI active = device().integrationsManager()->getActive();

    StaticJsonDocument<384> doc;

    // Product-specific top-level fields
    doc["deviceType"] = "iheater_link";
    doc["active"]     = idryer::cloud::activeIntegrationToString(active);
    doc["outputMode"] = heating ? 1 : 0;
    doc["targetTempC"]= cmd.targetTempC;

    // Standard units[] block — backend stores history from this
    // temperature/humidity = 0 if the device has no sensors
    JsonArray units = doc.createNestedArray("units");
    JsonObject u    = units.createNestedObject();
    u["unitId"]     = "U1";
    u["temperature"]= 0;
    u["humidity"]   = 0;
    u["heaterPower"]= heating ? 100 : 0;
    u["fanStatus"]  = false;

    mqtt->publishTelemetry(doc);  // timestamp is added automatically
}
```

## ステップ 3 — `loop()` から呼び出す

```cpp
void loop() {
    device().loop();

    static uint32_t s_lastTelMs = 0;
    if ((uint32_t)(millis() - s_lastTelMs) >= 5000u) {
        s_lastTelMs = millis();
        publishCustomTelemetry();
    }
    // ...
}
```

## してはいけないこと

- **両方を発行しないでください** idryer-core 自動テレメトリ（非ゼロ `telemetryPeriodMs`）とカスタムテレメトリを同時に。バックエンドは同じトピックで 2 つのメッセージを受け取って両方を処理するため、データが重複します。
- **`telemetryPeriodMs = 0` の場合、`device().publishTelemetryNow()` を呼ばないでください** — プロダクト固有のフィールドなしで標準削減ペイロードを発行します。

## ライブラリがこれを実装しない理由

idryer-core は既に `units[]` 内で `heaterPower: 1` を発行しており、加熱が有効であることを知るのに形式的には十分です。問題はライブラリではなくバックエンド（`telemetry.handler.ts`）にあります：トップレベルの `outputMode` フィールドを特に見て、標準的な `heaterPower` から `heaterIntent` を導出しません。これはバックエンド側の技術負債です。

現在のレシピは一時的な回避策です。バックエンドが `units[0].heaterPower` から `heaterIntent` を導出するように修正されれば、`telemetryPeriodMs = 5000` に戻して `publishCustomTelemetry()` を削除できます。標準ライブラリテレメトリは変更なしで動作します。

`telemetry.handler.ts` へのアップデートを監視してください：`heaterPower` のフォールバックがそこに追加されたら、このレシピは冗長になります。
