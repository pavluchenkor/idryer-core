# ステップ 01 — Improv を使用した WiFi プロビジョニング

このステップの後、ESP32 は WiFi に接続され、認証情報は NVS に保存されて、次回の再起動時に自動的に再接続できるようになります。ポータルと MQTT は次のステップで設定します。

## 必要なもの

**ハードウェア:**

- ESP32-C3 ボード (DevKit、Super Mini、またはそれと互換性のあるもの)
- USB ケーブル (ボードに応じて USB-C または Micro-USB)

**ソフトウェア:**

- VS Code の PlatformIO
- Chrome または Edge ブラウザ (Safari と Firefox は Web Serial API をサポートしていません)

## 手順

**1. プロジェクトのルートに `platformio.ini` を作成します:**

```ini
[env:improv-demo]
platform   = espressif32
framework  = arduino
board      = esp32-c3-devkitm-1

lib_deps =
    https://github.com/jnthas/Improv-WiFi-Library.git
    bblanchon/ArduinoJson @ ^6.21.3
    knolleary/PubSubClient @ ^2.8
    densaugeo/base64 @ ^1.4.0

build_flags =
    -DIDRYER_API_BASE='"https://portal.idryer.org/api"'
    -DMQTT_BROKER='"mqtt.idryer.org"'
    -DMQTT_PORT=8883
    -DMQTT_USE_TLS=1
```

`board` をボードの値に置き換えます (`esp32-c3-devkitm-1`、`seeed_xiao_esp32c3` など)。

**2. 例をコピーします。** [`examples/03_with_improv/03_with_improv.ino`](../../../examples/03_with_improv/03_with_improv.ino) の内容を取得し、プロジェクトの `src/main.cpp` として保存します。

**3. ChipFamily を設定します。** コピーしたファイルで、次の行を見つけます:

```cpp
s_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32_C3, ...);
```

ChipFamily がお使いのチップと一致していることを確認します: `CF_ESP32_C3`、`CF_ESP32_S3`、または `CF_ESP32`。

**4. フラッシュします:**

```bash
pio run -e improv-demo -t upload
```

**5. Chrome または Edge で [improv-wifi.com/serial](https://www.improv-wifi.com/serial/) を開きます。** **接続** をクリックし、ブラウザ ダイアログからデバイスの USB ポートを選択します。

**6. 2.4 GHz ネットワークの SSID とパスワードを入力します。** ウェブ ページは Serial-Improv 経由でボードに認証情報を送信します。ボードは NVS に保存します。

## 検証

Serial Monitor を開きます:

```bash
pio device monitor -b 115200
```

接続が成功すると、以下が表示されます:

```
[BOOT] WiFi connected, Improv done
[BOOT] IP: 192.168.1.42  RSSI: -47 dBm
```

この行が表示されない場合は、下のトラブルシューティング リンクを参照してください。

!!! note
    前回の実行から NVS に認証情報が既に保存されている場合、ボードは起動時に自動的に WiFi に接続します — Improv は不要です。

## 次は?

- [02-claim.md](02-claim.md) — デバイスを idryer.org アカウントにバインドします。
- [../../10-troubleshooting/01-troubleshooting.md](../10-troubleshooting/01-troubleshooting.md) — WiFi が接続しない場合。
