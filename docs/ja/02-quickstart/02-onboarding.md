# オンボーディング: 最初のデバイス クレーム

オンボーディングは、ESP32 が iDryer クラウドに登録され、あなたのアカウントにクレームされる 1 回限りの手順です。完了すると、デバイスはステータス「オンライン」、状態「準備完了」でポータルに表示され、その後のすべての電源オンは自動です。

## 必要なもの

- REPL ビルドでフラッシュされた ESP32 デバイス: env `esp32c3-super-mini-dev` ([5 分で始める](01-five-minutes.md)を参照) またはフラグ `IDRYER_DEV_REPL=1` を使用した任意の dev ビルド。
- USB ケーブル。
- [portal.idryer.org](https://portal.idryer.org/) のアカウント (開発の場合 — [staging.idryer.org](https://staging.idryer.org/))。

## パス 1. Serial REPL 経由 (推奨)

REPL はフラグ `IDRYER_DEV_REPL=1` を使用したビルドでのみ利用可能です。Serial Monitor を開き、3 つのコマンドを入力します — デバイスは WiFi に接続し、PIN をリクエストし、クレームの準備が整います。

### 1. dev ビルドをフラッシュします

```bash
pio run -e esp32c3-super-mini-dev -t upload
```

または、`-DIDRYER_DEV_REPL=1` が設定されている任意の env を使用します。

### 2. Serial Monitor を開きます

```bash
pio device monitor -b 115200
```

ブート後、プロンプトが表示されます:

```
[boot] iDryer dev REPL ready — type 'help'
```

直後に、クラウド スタック メッセージがログに表示されます:

```
[CLOUD] Init: serial=DEVICE_XXXXXXXXXXXX deviceId=(none)
[CLOUD] Connecting to WiFi...
```

### 3. WiFi に接続します

Serial Monitor コンソールに入力します:

```
wifi MyHomeWiFi MySecretPass
```

応答:

```
> wifi MyHomeWiFi MySecretPass
[wifi] saving 'MyHomeWiFi' / '****'
```

認証情報は NVS に書き込まれます。ボードは直ちに `WiFi.begin()` を呼び出します。ログには以下が表示されます:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -51 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

### 4. PIN を取得し、ポータルでクレームします

デバイスは自動的にプロビジョニングされ、7 桁の PIN が登録されます。PIN は 10 分間有効です。

1. [portal.idryer.org](https://portal.idryer.org/) を開きます (またはステージング)。
2. **デバイスを追加** に移動します。
3. Serial Monitor から PIN を入力します。

クレームが成功すると、ログには以下が表示されます:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

PIN の有効期限が切れた場合 — `claim` コマンドを実行して新しいコマンドを取得します。

### 有用な REPL コマンド

| コマンド | 機能 | 使用場面 |
|---------|------|---------|
| `help` | コマンド リストを表示 | 構文を思い出す |
| `status` | 現在の状態: WiFi、IP、RSSI、オンライン、シリアル | 接続診断 |
| `wifi <ssid> <password>` | WiFi 認証情報を NVS に保存して再接続 | 最初のオンボーディングまたはネットワーク変更 |
| `claim` | クレーム フローを手動で開始し、新しい PIN を取得 | PIN の有効期限が切れたまたは再クレームが必要 |
| `wipe` | NVS を消去 (認証情報、クレーム、メニュー) して再起動 | ファクトリー リセット |
| `restart` | ESP のソフトウェア再起動 | 物理的な接続を解除せずに快速再起動 |

## パス 2. Improv-WiFi 経由 (Web Serial)

Improv-WiFi はすべてのビルドに組み込まれており、`IDRYER_DEV_REPL` フラグに依存しません。デバイスをユーザーに引き継ぐ場合またはターミナルが不便な場合に適しています。Chrome または Edge が必要です — Safari と Firefox は Web Serial API をサポートしていません。

### 1. ボードがフラッシュされていることを確認します

任意の prod ビルドで問題ありません。Improv-WiFi は常にアクティブです。

### 2. ウェブ ページを開きます

[https://www.improv-wifi.com/serial/](https://www.improv-wifi.com/serial/) に移動し、**接続** をクリックして、ブラウザ ダイアログでデバイスの USB ポートを選択します。

### 3. SSID とパスワードを入力します

ページはネットワーク名とパスワードを要求し、Serial-Improv 経由でボードに送信します。ボードは認証情報を NVS に保存して WiFi に接続します。プロビジョニングと PIN の取得は自動的に行われます — パス 1 と同じです。

!!! note
    Improv-WiFi は `claim`、`wipe`、`status` をチェックすることはできません。手動クレーム フローと NVS 管理には REPL を使用します。

### 各パスをいつ使用するか

| 状況 | 推奨事項 |
|------|---------|
| ターミナルを開いた組込み開発者 | REPL |
| デバイスをユーザーに引き継ぐ | Improv-WiFi |
| `wipe` を手動で実行または `claim` を繰り返す必要がある | REPL |
| Safari または Firefox ブラウザ | REPL |
| PlatformIO がインストールされていない | Improv-WiFi |

## 問題が発生した場合

**PIN がログに表示されません。** デバイスが WiFi に接続したことを確認します: `status` を入力し、応答内の `ip=` フィールドが空でないことを確認します。WiFi がないとプロビジョニングは開始されません。

**PIN の有効期限が切れました。** `claim` コマンドを入力 — デバイスは新しい登録をリクエストして新しい PIN を出力します。

**デバイスは既に別のアカウントにクレームされています。** `wipe` を入力 — NVS は消去され、ボードが再起動してオンボーディングが最初から開始されます。

**ポータルが PIN を受け入れません。** 7 桁すべてをスペースなしでコピーしたこと、PIN が表示されてから 10 分未満が経過していることを確認します。

**Improv-WiFi がブラウザでデバイスを見つけません。** Chrome または Edge を使用していること、ESP32 USB ドライバーがインストールされていることを確認します。

## 次は?

- 完全な Link API: [../03-public-api/01-link-api-reference.md](../03-public-api/01-link-api-reference.md)
- センサーまたはペリフェラルを追加: [../04-patterns/](../04-patterns/)
