# UART 层

UART 模块实现了一个二进制帧协议，用于 ESP32（Link）和 RP2040（Controller）之间的双向通信。用于双 MCU 设备：iDryer LINK、iHeater LINK。

**存储链接不使用 UART 模块**——它是一个没有第二个 MCU 的独立 ESP32-C3 设备。

单独包含：

```cpp
#include <idryer_uart.h>
```

## 物理层

- UART 8N1、115200 波特率（默认）、无硬件流控制。
- 最大帧有效负载：200 字节。
- 每帧的 CRC-16/CCITT（多项式 0x1021、初始值 0xFFFF）。

## 帧结构

```
byte 0  : SOF = 0xAA
byte 1  : version = 1
byte 2  : flags   (需要 ACK | 是 ACK | 错误 | 片段 | 最后片段)
byte 3  : message kind (UartMsgKind)
byte 4  : sequence number (0..255, wrap)
byte 5  : payload length
payload : data (0..200 bytes)
crc16   : CRC, low byte + high byte
```

## 类

### UartBridge

模块的主要类。逐字节处理传入流、构建帧、验证 CRC、管理 ACK/重试，并将帧分派到注册的回调。

```cpp
UartBridge bridge;
bridge.begin(&Serial1, 115200);

// 在 begin() 之前注册处理程序
bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    UartHelloAckPayload ack{};
    bridge.sendHelloAck(ack);
});

// 在 loop() 中：
bridge.loop();
```

发送方法分为两组：

- ESP32 → RP2040：`sendHelloAck`、`sendCommand`、`sendProfileCommand`、`sendConfigPush`、`sendHeartbeat`、`sendClaimStatus`、`sendClaimComplete`、`sendWsStatus`、`sendTelemetryAck`、`sendCommandAck`、`sendConfigAck`。
- RP2040 → ESP32（或用于测试）：`sendHello`、`sendTelemetry`、`sendStatus`、`sendWeights`、`sendRfid`。

ACK/重试：具有 `UART_FLAG_ACK_REQ` 标志的帧重试最多 3 次，超时 700 毫秒。如果未收到 ACK — `send*` 返回 `false`。

### 消息类型

| 种类 | 代码 | 方向 | 目的 |
|------|------|------|------|
| `Hello` | 0x01 | RP2040 → ESP32 | 启动时公告；包含 MCU 序列号、设备类型、功能 |
| `HelloAck` | 0x02 | ESP32 → RP2040 | 响应 IP 地址和 SSID |
| `Telemetry` | 0x10 | RP2040 → ESP32 | 温度、湿度、加热器功率 |
| `Weights` | 0x12 | RP2040 → ESP32 | 秤读数 |
| `Status` | 0x13 | RP2040 → ESP32 | 当前干燥模式、会话进度 |
| `Rfid` | 0x14 | RP2040 → ESP32 | RFID 事件（标签检测/移除） |
| `Command` | 0x20 | ESP32 → RP2040 | 来自后端的命令（启动、停止、查找...） |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | 配置（简单或分块） |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | 正常运行时间、RSSI、云状态 |
| `Error` | 0x50 | 两个 | 协议错误 |
| `ClaimStart..Complete` | 0x70–0x72 | 两个 | 声称生命周期 |
| `WsEnable..StatusRequest` | 0x73–0x76 | 两个 | RP2040 上的 WebSocket 服务器控制 |

### ConfigReceiver / ConfigSender

用于通过 UART 在片段中传输大型 JSON 配置的实用程序类（每个片段 ≤ 194 字节数据）。

```cpp
// 接收（ESP32 ← RP2040）
ConfigReceiver rx;
bridge.setConfigChunkHandler([&rx, &mqtt](const UartConfigChunkPayload& p, uint8_t len, const UartFrameHeader& hdr) {
    if (rx.processFragment(p, len, hdr.flags) == ConfigFragResult::Complete) {
        mqtt.publishConfigRaw(rx.getJson(), rx.getLength());
        rx.reset();
    }
});

// 发送（ESP32 → RP2040）
ConfigSender tx;
uint16_t tid = ConfigSender::generateTransferId();
tx.send(json, length, tid, [&](const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
    return bridge.sendConfigPushChunk(p, payloadLen, flags);
});
```

## 与 CloudStateMachine 的集成

双 MCU 设备需要从 RP2040 获取序列号才能配置：

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

状态机保持在 `WaitingForMcuSerial` 直到调用 `setMcuSerial()`。
