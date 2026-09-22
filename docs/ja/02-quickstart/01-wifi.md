# Wi-Fi

ファームウェアにネットワークのパスワードはありません。保存されたネットワークのないデバイスは設定を待ち、受け取るとネットワークを NVS に保存して、以後は自分で接続します。2.4 GHz のネットワークのみです。

## iDryer アプリ（ESPTouch）

基本の方法：アプリがネットワークを無線で送ります。配線は不要です。

1. 電話はデバイスを使うネットワークにつながっている。
2. **新しいデバイスを接続** → **Wi-Fi** ステップ：ネットワーク名を確認し、パスワードを入力して **デバイスを接続** をタップ。
3. アプリは最大 90 秒間設定を送信し、**デバイスが接続されました** と表示する。

パスワードが違うと、デバイスは再び設定を待ちます。ステップをやり直してください。

## USB 経由の Improv

ネットワークがない間、コアはシリアルポートで Improv プロトコルを待ち受けます。

1. ボードを USB で接続します。
2. Chrome または Edge で [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) を開き（Web Serial は Safari と Firefox では動きません）、**Connect** を押してボードのポートを選びます。
3. ネットワーク名とパスワードを入力します。

この間ポートは Improv が使うため、ログはデバイスがネットワークにつながってから表示されます。Improv の動作中はシリアルモニターを閉じてください。

## コードで：開発用ベンチ

```cpp
void setup() {
    // 開発用ベンチ専用：NVS にまだネットワークがなければ保存する。
    s_link.seedWifiCredentialsIfEmpty("my-ssid", "my-password");
    s_link.begin();
}
```

`seedWifiCredentialsIfEmpty()` は NVS にまだネットワークがないときだけ書き込み、`setWifiCredentials()` は常に上書きします。`begin()` の前に呼んでください。

!!! warning
    パスワードをコードに入れたファームウェアを公開しないでください。ダウンロードした全員にパスワードが渡ります。

## 確認

ログ：

```text
[BOOT] WiFi ok, logs enabled
[INFO ] CLOUD: WiFi connected, IP: 192.168.1.42, RSSI: -55 dBm, …
```

## 次へ

[アカウントへの紐付け](02-claim.md)。
