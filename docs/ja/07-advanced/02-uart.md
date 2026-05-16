# UART レイヤー

UART モジュールは ESP32（Link）と RP2040（Controller）間の双方向通信用のバイナリ フレーム プロトコルを実装します。デュアル MCU デバイスで使用：iDryer LINK、iHeater LINK。

**Storage Link は UART モジュールを使用しません** — 第2 MCU のない自己完結型 ESP32-C3 デバイスです。

別途インクルード：

```cpp
#include <idryer_uart.h>
```

## 物理層

- UART 8N1、115200ボー（デフォルト）、ハードウェア フロー制御なし。
- 最大フレーム ペイロード：200バイト。
- CRC-16/CCITT（多項式 0x1021、初期値 0xFFFF）フレームごと。

## フレーム構造

```
byte 0  : SOF = 0xAA
byte 1  : version = 1
byte 2  : flags   (ACK required | is ACK | error | fragment | last fragment)
byte 3  : message kind (UartMsgKind)
byte 4  : sequence number (0..255, wrap)
byte 5  : payload length
payload : data (0..200 bytes)
crc16   : CRC, low byte + high byte
```

## クラス

### UartBridge

モジュールのメインクラス。受信ストリームをバイト単位で処理し、フレームを構築し、CRCを検証し、ACK/リトライを管理し、登録されたコールバックにフレームをディスパッチします。

```cpp
UartBridge bridge;
bridge.begin(&Serial1, 115200);

// Register handlers before begin()
bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    UartHelloAckPayload ack{};
    bridge.sendHelloAck(ack);
});

// in loop():
bridge.loop();
```

送信メソッドは2つのグループに分割されます：

- ESP32 → RP2040：`sendHelloAck`、`sendCommand`、`sendProfileCommand`、`sendConfigPush`、`sendHeartbeat`、`sendClaimStatus`、`sendClaimComplete`、`sendWsStatus`、`sendTelemetryAck`、`sendCommandAck`、`sendConfigAck`。
- RP2040 → ESP32（またはテスト用）：`sendHello`、`sendTelemetry`、`sendStatus`、`sendWeights`、`sendRfid`。

ACK/リトライ：`UART_FLAG_ACK_REQ` フラグを持つフレームは700 msのタイムアウト付きで最大3回リトライされます。ACKが受け取られない場合、`send*` は `false` を返します。

### メッセージタイプ

| 種類 | コード | 方向 | 目的 |
|------|------|-----------|---------|
| `Hello` | 0x01 | RP2040 → ESP32 | スタートアップでの発表；MCU シリアル、デバイス タイプ、機能を含む |
| `HelloAck` | 0x02 | ESP32 → RP2040 | IP アドレスと SSID を含む応答 |
| `Telemetry` | 0x10 | RP2040 → ESP32 | 温度、湿度、ヒーター電力 |
| `Weights` | 0x12 | RP2040 → ESP32 | スケール読み値 |
| `Status` | 0x13 | RP2040 → ESP32 | 現在の乾燥モード、セッション進捗 |
| `Rfid` | 0x14 | RP2040 → ESP32 | RFID イベント（タグ検出/除去） |
| `Command` | 0x20 | ESP32 → RP2040 | バックエンドからのコマンド（開始、停止、検索...） |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | 設定（単純またはチャンク） |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | アップタイム、RSSI、クラウド状態 |
| `Error` | 0x50 | both | プロトコル エラー |
| `ClaimStart..Complete` | 0x70–0x72 | both | ライフサイクルのクレーミング |
| `WsEnable..StatusRequest` | 0x73–0x76 | both | RP2040の WebSocket サーバーコントロール |

### ConfigReceiver / ConfigSender

大きなJSON設定をフラグメント（各フラグメント ≤ 194バイト）でUART上で転送するためのユーティリティ クラス。

```cpp
// Receive (ESP32 ← RP2040)
ConfigReceiver rx;
bridge.setConfigChunkHandler([&rx, &mqtt](const UartConfigChunkPayload& p, uint8_t len, const UartFrameHeader& hdr) {
    if (rx.processFragment(p, len, hdr.flags) == ConfigFragResult::Complete) {
        mqtt.publishConfigRaw(rx.getJson(), rx.getLength());
        rx.reset();
    }
});

// Send (ESP32 → RP2040)
ConfigSender tx;
uint16_t tid = ConfigSender::generateTransferId();
tx.send(json, length, tid, [&](const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
    return bridge.sendConfigPushChunk(p, payloadLen, flags);
});
```

## CloudStateMachine との統合

デュアル MCU デバイスはプロビジョニング前に RP2040 からシリアル番号が必要です：

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

状態機械は `setMcuSerial()` が呼ばれるまで `WaitingForMcuSerial` に留まります。
