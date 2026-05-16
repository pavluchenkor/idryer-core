# 詳細なセットアップ

初めてここに来た場合 — [5 分で始める](01-five-minutes.md) に移動; このページは高度なセットアップとトラブルシューティングをカバーしています。

簡潔なパス: ライブラリをワイヤーアップし、例をフラッシュし、点滅 LED とポータルのデバイスを見ます。

## 準備するもの

- ESP32 ボード (推奨: ESP32-C3 DevKit、Super Mini、XIAO ESP32-S3、Waveshare ESP32-S3 Zero)。
- PlatformIO with framework `arduino`、platform `espressif32`。
- WiFi 2.4 GHz インターネット アクセス。
- クレーム用 [portal.idryer.org](https://portal.idryer.org/) のアカウント。

## ステップ 1. ライブラリをワイヤーアップします

製品の `platformio.ini` で:

```ini
[env:my-device]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    file://../../lib/idryer-core
    bblanchon/ArduinoJson @ ^6.21.0
    knolleary/PubSubClient
    links2004/WebSockets             ; only needed for mqtt_with_local_ws

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_USE_TLS=1
```

## ステップ 2. `secrets.h` を作成します

[`examples/secrets.h.example`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/secrets.h.example) をプロジェクトの `include/secrets.h` にコピーし、SSID/パスワードを入力します。ファイルは `.gitignore` に含まれている必要があります。

```cpp
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
```

`IDRYER_API_BASE` は通常 `build_flags` 経由で設定され、secrets.h 経由ではありません。

## ステップ 3. 最初の例を開きます

最も単純なのは [`examples/01_blink_status/01_blink_status.ino`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/01_blink_status/01_blink_status.ino) です。これを出発点としてコピーします:

- センサー、ペリフェラル、LAN WS は不要です。
- `handleCommand` の手動は不要です — `IdryerRuntime` の組み込みフォールバックは基本コマンドを処理します。
- LED はデバイスがオンラインの場合に点滅します — それが成功インジケーターです。

## ステップ 4. フラッシュして観察します

```bash
pio run -e my-device -t upload
pio device monitor -b 115200
```

予期される ログシーケンス:

```
[CSM] state: Idle → WifiConnecting
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim     ← waiting for claim
[CSM] PIN: 1234567   expires in 600s          ← if auto-claim is enabled
...
[CSM] state: AwaitingClaim → Ready
[CSM] state: Ready → MqttConnecting
[CSM] state: MqttConnecting → Online          ← ready, LED starts blinking
[RT]  Cloud Online
```

## ステップ 5. デバイスをアカウントにクレームします

自動クレームは既に例で有効になっています。PIN はログに表示されます。[portal.idryer.org](https://portal.idryer.org/) → "デバイスを追加" で入力します。クレーム後、`CloudStateMachine` は `Online` に遷移します。

## 次に何をするか

次の例は、それぞれ 1 つの新しいレベルの複雑さを導入します:

| 例 | 追加されるもの |
|-----|--------------|
| [`minimal_mqtt_only`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/minimal_mqtt_only/minimal_mqtt_only.ino) | custom `handleCommand`、`commands/invoke` と `commands/set` の処理 |
| [`03_with_improv`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/03_with_improv/03_with_improv.ino) | Improv 経由の WiFi プロビジョニング (ハードコードされた認証情報なし) |
| [`mqtt_with_local_ws`](https://github.com/pavluchenkor/idryer-core/blob/main/examples/mqtt_with_local_ws/mqtt_with_local_ws.ino) | ローカル LAN WebSocket サーバー + `DevicePublisher` (1 つの発行 — 2 つのトランスポート) |

## Dev REPL via Serial (ポータルなし、ブラウザなし)

開発者向けの代替パス — Improv と Portal UI なしで、標準 Serial Monitor で完全なクレーム フローを直接見ます。

`platformio.ini` で、フラグ `-DIDRYER_DEV_REPL=1` を使用して dev 環境を作成します:

```ini
[env:my-device-dev]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1
build_flags =
    ${env:my-device.build_flags}
    -DIDRYER_DEV_REPL=1
```

フラグが有効にするもの:
- HAL ログは `Serial` に**直ちに**ブートから開始されます (WiFi が接続するまで沈黙なし)。
- Improv プロビジョニングは**無効**です — Serial は対話型コマンドに無料です。
- `main.cpp` に単純な REPL が表示されます: `wifi`、`claim`、`status`、`wipe`、`restart`、`help`。

完全なフロー:

```bash
pio run -e my-device-dev -t upload
pio device monitor -b 115200
```

モニターで:

```
[boot] iDryer dev REPL ready — type 'help'
> wifi MyHomeWiFi MyPassword
[wifi] saving 'MyHomeWiFi' / '****'
[CSM] state: WifiConnecting → Provisioning
[CSM] state: Provisioning → AwaitingClaim
> claim
CLAIM_PIN:1234567:600
[claim] PIN=1234567, valid 600 s — enter in portal
[CSM] state: AwaitingClaim → Ready → Online
> status
[status] wifi=3 ip=192.168.0.140 rssi=-44 online=1 serial=DEVICE_AABBCCDDEEFF
> wipe
[wipe] erasing NVS + reboot…
```

REPL は Serial monitor のライン ending 設定 (`\n`、`\r`、またはアイドル タイムアウト 120 ms) に関係なくコマンドを受け入れます — `pio device monitor`、Arduino IDE Serial Monitor、`screen`、`picocom` を含む任意のターミナルで動作します。

プロダクション ビルド (`-e my-device-prod`、`IDRYER_DEV_REPL` なし) は Improv via Chrome (`https://www.improv-wifi.com/`) を使用し、REPL コードを含みません — フラグはコンパイル時で、Flash を節約します。

`secrets.h` with `WIFI_SSID/WIFI_PASSWORD` (ステップ 2) は、ヘッドレス CI/オート フラッシュ シナリオのための別のパスです — 両方の環境で動作します。

例のいずれかが実行されたら、以下を読みます:

- [05-architecture/01-composition-root.md](../05-architecture/01-composition-root.md) — `main.cpp` でのオブジェクト順序。
- [05-architecture/03-data-flow.md](../05-architecture/03-data-flow.md) — データの移動方法。
- [04-patterns/](../04-patterns/) — レシピ: センサー追加、ペリフェラル、トランスポート追加。
- [09-add-product/01-add-new-product.md](../09-add-product/01-add-new-product.md) — 新しいプロダクトの完全なチェックリスト。
- [10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — スタックが詰まった場合の対処。
