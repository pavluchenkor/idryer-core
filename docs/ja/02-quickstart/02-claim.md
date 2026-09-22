# アカウントへの紐付け

紐付けは一度だけの手順です。デバイスは使い捨ての紐付けトークンを受け取り、ポータルで恒久的なシークレットと交換し、シークレットを NVS に保存します。以後は再起動のたびに自分でポータルに接続します。シークレットがない間、デバイスは設定モードでトークンを待ちます。

## iDryer アプリで

1. デバイスはネットワーク上にあり（[Wi-Fi](01-wifi.md) を参照）、電話も同じネットワークにある。
2. **新しいデバイスを接続** → **ペアリング** ステップ（デバイスがすでにネットワーク上なら、ウィンドウ上部のステップのチップをタップ）→ **ペアリング**。
3. アプリはローカルネットワークでデバイスを見つけ、ポータルからトークンを取得してデバイスに渡し、デバイスがオンラインになったことをポータルが確認するまで待つ。
4. **ペアリングが完了しました** の後、デバイスはポータルとアプリの一覧にある。

紐付け前のログ：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
…
[INFO ] CLOUD: binding-v3: no secret — awaiting pairing token (SETUP)
```

紐付け後：

```text
[INFO ] CLOUD: binding-v3: pairing token received (… chars)
[INFO ] CLOUD: binding-v3: activating with pairing token (serial=DEVICE_… mcu=-)
[INFO ] CLOUD: binding-v3: activated, deviceId=… -> Ready
…
[INFO ] MQTT: Connected! …
```

ファームウェアの Web インストーラーは、USB 経由で `PAIR_TOKEN` コマンドによりトークンを渡します（下記参照）。

## 紐付け解除

アプリまたはポータルでの紐付け解除は、`revoke` コマンドとしてデバイスに届きます。どのファームウェアにもハンドラーが必要です：

```cpp
// アプリまたはポータルでの紐付け解除：シークレットを消去し、新しい紐付けを待つ。
s_link.onCommand("revoke", [](JsonObjectConst) { s_link.handleRevoke(); });
```

`handleRevoke()` はシークレットを消去し、ネットワークは残します。デバイスは再び紐付けを待ちます。ハンドラーがないとシークレットはデバイスに残り、`WIPE_IDENTITY` の後でないと再び紐付けできません。

## USB コマンド

ネットワーク接続後、コアはシリアルポートで行を受け付けます（115200、各行は改行で終わる）：

| コマンド | 応答 | 動作 |
|---|---|---|
| `STATUS` | `STATUS:state=… wifi=… ip=… cloud=… serial=… mcu=… fw=… part=…` | 状態：`state` は `bound`（紐付け済み）または `setup`（トークン待ち）、`cloud` は `online`/`offline` |
| `WIPE_IDENTITY` | `WIPE_IDENTITY:OK` | `revoke` と同じくシークレットを消去。ネットワークは残る |
| `PAIR_TOKEN:<トークン>` | `PAIR_TOKEN:OK`、`PAIR_TOKEN:ERROR`、`PAIR_TOKEN:ERROR:ALREADY_BOUND` | 紐付けトークンを渡す。紐付け済みのデバイスは受け付けない |

```text
STATUS
STATUS:state=setup wifi=1 ip=192.168.1.42 cloud=offline serial=DEVICE_… mcu=- fw=0.1.0 part=…
```

ネットワーク接続前はポートを Improv が使うため、コマンドは応答しません。`IDRYER_DEV_REPL` 付きのビルドではポートは製品のもので、これらのコマンドはありません。[詳細設定](99-detailed-setup.md) を参照。

## うまくいかないとき

- アプリがデバイスを見つけない：電話とデバイスを同じネットワークに。ゲストネットワークはデバイス検出をよくブロックします；
- `PAIR_TOKEN:ERROR:ALREADY_BOUND`、またはデバイスが別のアカウントのもの：ポータルで紐付けを解除するか `WIPE_IDENTITY` を送り、改めて紐付けます。

## 次へ

[テレメトリ](03-telemetry.md)。
