# idryer-coreの動作方法

idryer-coreはESP32用のライブラリで、クラウドスタック全体を処理します：Improv-Serialを使用したWiFiプロビジョニング、デバイスをidryer.orgアカウントにバインドするクレームプロトコル、自動再接続を備えたTLS MQTTセッション、ポータルからのコマンドルーティング、定期的なテレメトリ発行。

デバイス固有の記述のみを行います：センサーの読み取り、周辺機器の駆動。その他すべてはライブラリ内です。

## mqtt_contract.yaml — 真実の唯一の情報源

ファイル [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) は以下を定義します：

- **capabilities** — 各デバイスタイプがサポートする周辺機器（ヒーター、LEDストリップ、センサー）；
- **telemetry fields** — MQTTパケット内のフィールド名とデータ型；
- **UART protocol** — ESP32とコプロセッサ間の構造；
- **TypeScript types** — ポータルフロントエンド用。

このファイルから、コードは自動的に生成されます：

| 生成される内容 | 場所 |
|---|---|
| `iDryer::Config`（has* フラグ） | `src/_generated/iDryer_api.h` |
| MQTTトピック（C++ 定数） | `contracts/_generated/mqtt_topics.h` |
| TypeScript型 | `contracts/_generated/mqtt-api.types.ts` |

!!! warning
    `src/_generated/` および `contracts/_generated/` 内のファイルを手動で編集しないでください — 次の再生成実行時に上書きされます。

## 新しい周辺機器を追加する方法

手順は新しい機能（ボタン、CO2センサー、RFIDリーダー）と同じです。

**1.** [`contracts/mqtt_contract.yaml`](../../../contracts/mqtt_contract.yaml) の `capability_vocabulary` にエントリを追加します：

```yaml
co2:
  json_key: "co2"
  config_flag: "hasCo2"
  telemetry_field: "co2Ppm"
  telemetry_type: "uint16_t"
  description: "CO2 sensor (ppm)"
```

**2.** 再生成を実行します：

```bash
cd contracts
./regen.sh
```

この後、`iDryer::Config` は `hasCo2` フィールドを持ち、TypeScript は `HardwareUnitConfigCapabilities.co2` を持つようになります。

**3.** デバイスの `main.cpp` でフラグを設定します：

```cpp
static const iDryer::Config CFG = {
    // ...
    .hasCo2 = true,
};
```

**4.** デバイスをフラッシュします。ポータルはMQTT `/info` トピックから `co2: true` を読み取り、ポータル側の変更なしで対応するUIブロックを自動的に表示します。

まだ契約にない周辺機器タイプについては、idryer-coreリポジトリへPRを開き、`capability_vocabulary` にエントリを追加します。マージ後 — `regen.sh` を実行します。

## このライブラリで構築された2つの本番製品

**iDryer Storage Link** — WS2812B LEDストリップとSHT31温度/湿度センサー付きESP32-C3。

**iHeater Link** — iHeaterヒーターへのRMT出力を備えたESP32-C3、Bambu Lab、Klipper/Moonraker、Home Assistantの統合。

両製品はPlatformIO `lib_deps` を介してidryer-coreを含め、製品固有のロジックのみを実装します。

## 次に行くべき場所

- [01-wifi.md](01-wifi.md) — Improv-Serialを使用してESP32をWiFiに接続します。
- [../../../README.md](../../../README.md) — ライブラリ概要とコード生成リファレンス。
