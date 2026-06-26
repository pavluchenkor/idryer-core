# UART 层

UART 模块实现 ESP32（Link）和 RP2040（Controller）之间的双向二进制帧协议。用于双 MCU 设备：iDryer LINK、iHeater LINK。

**Storage Link 不使用 UART 模块** — 它是没有第二个 MCU 的独立 ESP32-C3 设备。

单独 include：

```cpp
#include <idryer_uart.h>
```

## 物理层

- UART 8N1，115200 baud（默认），无硬件流控。
- 最大帧 payload：200 字节。
- 每帧 CRC-16/CCITT（poly 0x1021，init 0xFFFF）。

## 帧结构

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

## 类

### UartBridge

模块主类。逐字节处理传入流、构建帧、校验 CRC、管理 ACK/重试，并把帧分发给注册的 callback。

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

发送方法分为两组：

- ESP32 → RP2040：`sendHelloAck`、`sendCommand`、`sendProfileCommand`、`sendHeartbeat`、`sendClaimStatus`、`sendClaimComplete`、`sendWsStatus`、`sendTelemetryAck`、`sendCommandAck`、`sendConfigAck`。
- RP2040 → ESP32（或测试用）：`sendHello`、`sendTelemetry`、`sendStatus`、`sendWeights`、`sendRfid`。

ACK/重试：带 `UART_FLAG_ACK_REQ` 标志的帧最多重试 3 次，超时 700 ms。如果没有收到 ACK，`send*` 返回 `false`。

### 消息类型

| Kind | Code | Direction | Purpose |
|------|------|-----------|---------|
| `Hello` | 0x01 | RP2040 → ESP32 | 启动通告；包含 MCU 序列号、设备类型、能力 |
| `HelloAck` | 0x02 | ESP32 → RP2040 | 带 IP 地址和 SSID 的响应 |
| `Telemetry` | 0x10 | RP2040 → ESP32 | 温度、湿度、加热功率 |
| `Weights` | 0x12 | RP2040 → ESP32 | 称重读数 |
| `Status` | 0x13 | RP2040 → ESP32 | 当前干燥模式、会话进度 |
| `Rfid` | 0x14 | RP2040 → ESP32 | RFID 事件（标签检测/移除） |
| `Command` | 0x20 | ESP32 → RP2040 | 来自后端的命令（start、stop、find...） |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | 配置（简单或分块） |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | Uptime、RSSI、云端状态 |
| `Error` | 0x50 | both | 协议错误 |
| `ClaimStart..Complete` | 0x70–0x72 | both | Claim 生命周期 |
| `WsEnable..StatusRequest` | 0x73–0x76 | both | RP2040 上的 WebSocket 服务器控制 |

### ConfigReceiver / ConfigSender

用于通过 UART 分片传输大 JSON 配置的辅助类（每片数据 ≤ 194 字节）。

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

## 与 CloudStateMachine 的集成

双 MCU 设备在 provisioning 前需要先拿到 RP2040 的序列号：

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

状态机会停留在 `WaitingForMcuSerial`，直到调用 `setMcuSerial()`。
