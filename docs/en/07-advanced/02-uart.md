# UART layer

The UART module implements a binary frame protocol for bidirectional communication between ESP32 (Link) and RP2040 (Controller). Used in dual-MCU devices: iDryer LINK, iHeater LINK.

**Storage Link does not use the UART module** — it is a standalone ESP32-C3 device without a second MCU.

Include separately:

```cpp
#include <idryer_uart.h>
```

## Physical layer

- UART 8N1, 115200 baud (default), no hardware flow control.
- Maximum frame payload: 200 bytes.
- CRC-16/CCITT (poly 0x1021, init 0xFFFF) per frame.

## Frame structure

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

## Classes

### UartBridge

Main class of the module. Processes the incoming stream byte by byte, builds frames, verifies CRC, manages ACK/retry, and dispatches frames to registered callbacks.

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

Send methods are split into two groups:

- ESP32 → RP2040: `sendHelloAck`, `sendCommand`, `sendProfileCommand`, `sendConfigPush`, `sendHeartbeat`, `sendClaimStatus`, `sendClaimComplete`, `sendWsStatus`, `sendTelemetryAck`, `sendCommandAck`, `sendConfigAck`.
- RP2040 → ESP32 (or for tests): `sendHello`, `sendTelemetry`, `sendStatus`, `sendWeights`, `sendRfid`.

ACK/retry: frames with the `UART_FLAG_ACK_REQ` flag are retried up to 3 times with a 700 ms timeout. If no ACK is received — `send*` returns `false`.

### Message types

| Kind | Code | Direction | Purpose |
|------|------|-----------|---------|
| `Hello` | 0x01 | RP2040 → ESP32 | Announcement at startup; contains MCU serial, device type, capabilities |
| `HelloAck` | 0x02 | ESP32 → RP2040 | Response with IP address and SSID |
| `Telemetry` | 0x10 | RP2040 → ESP32 | Temperature, humidity, heater power |
| `Weights` | 0x12 | RP2040 → ESP32 | Scale readings |
| `Status` | 0x13 | RP2040 → ESP32 | Current drying mode, session progress |
| `Rfid` | 0x14 | RP2040 → ESP32 | RFID event (tag detected/removed) |
| `Command` | 0x20 | ESP32 → RP2040 | Command from backend (start, stop, find...) |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | Configuration (simple or chunked) |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | Uptime, RSSI, cloud state |
| `Error` | 0x50 | both | Protocol error |
| `ClaimStart..Complete` | 0x70–0x72 | both | Claiming lifecycle |
| `WsEnable..StatusRequest` | 0x73–0x76 | both | WebSocket server control on RP2040 |

### ConfigReceiver / ConfigSender

Utility classes for transferring large JSON configs over UART in fragments (each fragment ≤ 194 bytes of data).

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

## Integration with CloudStateMachine

Dual-MCU devices require the serial number from RP2040 before provisioning:

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

The state machine stays in `WaitingForMcuSerial` until `setMcuSerial()` is called.
