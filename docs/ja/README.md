# idryer-core — ライブラリドキュメント

`idryer-core` — ESP32ベースのiDryerデバイス向けC++ライブラリ(Arduino/PlatformIO)。WiFi、MQTT、クラウド状態マシン、およびコマンドルーティングを管理します。製品はデバイス固有の動作のみを実装します。

これは**ライブラリ**のドキュメントであり、特定の製品のドキュメントではありません。
製品ドキュメントは[`docs/ru/`](../../docs/ru/)にあります。

---

## クイックスタート

**実装する3つのもの：**

1. `IProfile`を実装する — 5つのメソッド(config、info、loop)。
2. `main.cpp`を組み立てる — 静的オブジェクト、コンストラクタ経由で依存関係を渡します。
3. `handleCommand`を登録する — MQTT用および、オプションでローカルWS用の単一ハンドラ。

**ライブラリが行う3つのこと：**

1. WiFi → プロビジョニング → MQTTセッションを管理します。
2. 受信コマンドを`handleCommand`にルーティングします(`ping`を除く、これは内部で処理されます)。
3. 適切なタイミングで`IProfile`メソッドを呼び出します。

**変更しないでおくことができるもの：**

- `ArduinoWifiManager`、`ArduinoCredentialStore`、およびその他の`Arduino*`クラス — そのまま使用、サブクラス化は不要です。
- `CloudStateMachine` — それを作成して`IdryerRuntime`に渡します。そこから自動管理されます。
- `ActionDispatcher` — invoke/setの互換性フォールバック。新しい製品の場合、コマンド処理は`ActionDispatcher`ではなく`setCommandHandler()`を経由します。

実践ガイド: [09-add-product/01-add-new-product.md](09-add-product/01-add-new-product.md)

動作する例: [`examples/`](../../examples/)

---

## セクション

| セクション | 説明 |
|---------|-------------|
| [01-overview/01-what-is-idryer-core](01-overview/01-what-is-idryer-core.md) | ライブラリの目的、実装されていないもの、使用者 |
| [01-overview/02-module-map](01-overview/02-module-map.md) | すべてのモジュールの表：目的、オプション性 |
| [02-getting-started](02-quickstart/01-five-minutes.md) | 新しい開発者向けの短い入門：配線、フラッシュ、期待値 |
| [05-architecture/01-composition-root](05-architecture/01-composition-root.md) | 製品がスタックを組み立てる方法：オブジェクト作成順序、main.cppパターン |
| [05-architecture/02-library-vs-product-boundary](05-architecture/02-library-vs-product-boundary.md) | ライブラリに存在するもの、製品に存在するもの |
| [05-architecture/03-data-flow](05-architecture/03-data-flow.md) | 稼働中デバイスのデータフロー：受信コマンド、送信メッセージ、接続 |
| [06-mqtt/01-mqtt-client](06-mqtt/01-mqtt-client.md) | `MqttClient`クラス：コンストラクタ、接続、発行 |
| [06-mqtt/02-topics-and-messages](06-mqtt/02-topics-and-messages.md) | すべてのMQTTトピック：文字列、ペイロード、保持、QoS |
| [04-runtime/01-idryer-runtime](07-advanced/01-runtime.md) | `IdryerRuntime`：コーディネート内容、処理するコマンド |
| [05-uart/01-uart-layer](07-advanced/02-uart.md) | デュアルMCUデバイス用UARTブリッジ |
| [06-integrations/01-integrations-overview](07-advanced/03-integrations.md) | Bambu、Home Assistant、Moonraker：セットアップ、制限事項 |
| [07-platform-arduino/01-arduino-platform](07-advanced/04-platform-arduino.md) | デバイスインターフェースのArduino実装 |
| [08-profiles-and-products/01-profiles-model](07-advanced/05-profiles.md) | `IProfile`インターフェース、コールバック、`LedStripProfile`の例 |
| [09-contracts/01-mqtt-contract](08-contracts/01-mqtt-contract.md) | `mqtt_contract.yaml`：目的および変更ルール |
| [10-how-to-add-product/01-add-new-product](09-add-product/01-add-new-product.md) | `idryer-core`の上に新しい製品を構築するためのチェックリスト |
| [10-troubleshooting](10-troubleshooting/01-troubleshooting.md) | 一般的な問題：WiFi、プロビジョニング、MQTT、コマンド、LocalAccess |
| [04-patterns/01-add-sensor](04-patterns/01-add-sensor.md) | センサー(データソース)を追加し、その読み取り値を発行する方法 |
| [04-patterns/02-add-peripheral](04-patterns/02-add-peripheral.md) | ペリフェラルを追加し、コマンドを受け取る方法 |
| [04-patterns/03-add-transport](04-patterns/03-add-transport.md) | 並列トランスポート(BLE、HTTP、カスタム)を追加する方法 |
| [04-patterns/04-data-flow](04-patterns/99-data-flow.md) | センサー/ペリフェラル/プロファイル/パブリッシャー間のデータ渡しの実践的レシピ |
