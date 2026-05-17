# ステップ 02 — クレーム: ポータルへのバインディング

このステップの後、デバイスはあなたの [portal.idryer.org](https://portal.idryer.org/) アカウントにステータス「オンライン」で表示されます。その後のすべての再起動は自動です — 再度のクレームは不要です。

## クレームとは

クレームは、ESP32 が idryer.org クラウドに登録され、あなたのアカウントにバインドされる 1 回限りの手順です。デバイスは 10 分間有効な 7 桁の PIN を生成します。PIN をポータルに入力します — バインディングは完了です。

クレーム後、`deviceId` が NVS に保存されます — これはクラウド内のデバイスの一意の識別子です。その後の再起動では、ESP32 はクレーム フローを繰り返さずに MQTT に直接接続します。

## 必要なもの

- [ステップ 01](01-wifi.md) からフラッシュされた ESP32 で WiFi に接続された状態
- [portal.idryer.org](https://portal.idryer.org/) のアカウント
- USB ケーブルと開いた Serial Monitor

## 手順

**1. スケッチに自動クレームが含まれていることを確認します。** 次の行は `setup()` に含まれている必要があります (`03_with_improv` の例には既に存在しています):

```cpp
s_cloud.setUnclaimedCallback([](void*) { s_cloud.requestClaim(); }, nullptr);
```

このコールバックは、デバイスがインターネットに到達し、まだクレームされていないことが検出されたときに自動的に発火します。

**2. Serial Monitor を開き、** ボードを再起動します:

```bash
pio device monitor -b 115200
```

**3. ログで PIN を待ちます。** WiFi → プロビジョニング → クレーム待機の後:

```
[CLOUD] WiFi connected, IP: 192.168.1.42, RSSI: -47 dBm
[CLOUD] Provisioning device...
[CLOUD] Provision OK: isNew=1 isClaimed=0
[CLOUD] Registering device for claim...
[CLOUD] PIN: 3847291 (expires in 600s)
```

デバイスは待機しています。PIN は 10 分間有効です。

**4. [portal.idryer.org](https://portal.idryer.org/) に移動し、** **デバイスを追加** に移動します。

**5. Serial Monitor から PIN を入力します** (7 桁、スペースなし)。

**6. ポータルでバインディングを確認します。** Serial Monitor には次のように表示されます:

```
[CLOUD] Device claimed! deviceId=...
[CLOUD] Connecting to MQTT...
[CLOUD] MQTT connected!
[RT] Cloud Online
```

## 検証

ポータルのデバイス リストを開きます — デバイスはステータス **オンライン** で表示されます。内蔵 LED は 500 ms ごとに 1 回点滅します (`01_blink_status` の例を使用している場合)。

!!! note
    PIN の有効期限が切れた場合 (10 分以上経過) — ボードを再起動します。自動クレームは新しい PIN を生成します。

!!! warning
    デバイスが既に別のアカウントでクレームされている場合、`IDRYER_DEV_REPL=1` を有効にして Serial Monitor に `wipe` コマンドを入力します。NVS は消去され、ボードが再起動され、クレームは最初から開始されます。

## 次は?

- [03-telemetry.md](03-telemetry.md) — センサーを接続し、ポータルに読み取り値を発行します。
- [02-onboarding.md](02-onboarding.md) — REPL と Improv パスの詳細なオンボーディング ドキュメント。
