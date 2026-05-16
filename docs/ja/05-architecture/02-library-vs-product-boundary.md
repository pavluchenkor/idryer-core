# 境界：ライブラリとプロダクト

## ライブラリに含まれるもの

ライブラリ（`lib/idryer-core/`）には以下が含まれます：

- ネットワークスタック全体：WiFi、HTTP、MQTT、TLS。
- プロビジョニング/クレーミングプロトコル。
- クラウド状態機械（`CloudStateMachine`）。
- UARTブリッジとフレームプロトコル。
- 統合クライアント（Bambu、HA、Moonraker）。
- デバイスインターフェース（`IWifiManager`、`ICredentialStore`、`IHttpClient`、`IProfile`）。
- これらのインターフェースのArduino実装。
- MQTTトピックおよび発行/サブスクリプション ロジック。

ライブラリに属するコードをテストします：**変更せずに、任意のハードウェアを持つ任意のプロダクトが使用できます**。

## プロダクトに含まれるもの

プロダクト（`src/`）には以下が含まれます：

- `IProfile` 実装 — 設定、情報ペイロード、`applyConfig`。
- デバイス固有のビジネス ロジック（LED制御、乾燥、加熱）。
- `onInvoke` / `onSetCommand` ハンドラー。
- プロダクト センサーおよびテレメトリ発行。
- ペリフェラル初期化（FastLED、Wire、ImprovWiFi）。
- `main.cpp` のコンポジション ルート。

プロダクトに属するコードをテストします：**ハードウェアまたは設定を変更しないと、それは無意味です**。

## 具体的な例

| コード | 場所 | 理由 |
|------|---------------|-----|
| `MqttClient` | ライブラリ | すべてのプロダクトが MQTT を必要とします |
| `CloudStateMachine` | ライブラリ | プロビジョニング/クレーミングはすべてに対して同じ |
| `ArduinoWifiManager` | ライブラリ | WiFi接続はプロダクトに依存しません |
| `LedStripProfile` | プロダクト | Storage Link に固有 TODO: use consistent Storage name throughout the doc |
| `LedStripExecutor` | プロダクト | FastLED を制御し、他のデバイスでは不要 |
| `Sht31ClimateSensor` | プロダクト | 特定のプロダクト向けの特定センサー |
| `StorageTelemetryPublisher` | プロダクト | Storage Link テレメトリフォーマットを知っています |
| `IProfile` | ライブラリ | ライブラリが呼ぶコントラクト |
| `BambuClient` | ライブラリ | 統合は iDryer と iHeater に再利用されます |

## インターフェースとしての境界

ライブラリはプロダクトを `IProfile` を通じてのみ知っています。すべての相互作用は 5 つのメソッドで実行されます：

```cpp
profile->onOnline();               // library → product: first time going online
profile->loop();                   // library → product: every cycle
profile->buildInfoJson(buf, len);  // library → product: info payload needed
profile->getConfig(doc);           // library → product: config needed
profile->applyConfig(id, val);     // library → product: set command received
```

プロダクトはライブラリを `MqttClient`（テレメトリ/イベント発行用）およびコマンドの `ActionDispatcher` コールバック（用）を通じて知っています。

## 境界を越えてはいけないもの

- ライブラリはプロダクトヘッダーをインクルードしてはいけません。
- プロダクトは `CloudStateMachine::handleProvisioning()` またはその他のプライベートスタック メソッドを直接呼んではいけません。公開APIを通じてのみ。
- プロダクト テレメトリは `s_mqtt.publishTelemetry()` を通じて直接発行されます。ランタイムはそれを見ません。
