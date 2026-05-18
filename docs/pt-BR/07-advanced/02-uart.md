# Camada UART

O módulo UART implementa um protocolo de quadro binário para comunicação bidirecional entre ESP32 (Link) e RP2040 (Controller). Usado em dispositivos dual-MCU: iDryer LINK, iHeater LINK.

**Storage Link não usa o módulo UART** — é um dispositivo ESP32-C3 independente sem um segundo MCU.

Incluir separadamente:

```cpp
#include <idryer_uart.h>
```

## Camada física

- UART 8N1, 115200 baud (padrão), sem controle de fluxo de hardware.
- Payload máximo do quadro: 200 bytes.
- CRC-16/CCITT (poly 0x1021, init 0xFFFF) por quadro.

## Estrutura do quadro

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

Classe principal do módulo. Processa o stream de entrada byte a byte, constrói quadros, verifica CRC, gerencia ACK/retry e envia quadros para callbacks registrados.

```cpp
UartBridge bridge;
bridge.begin(&Serial1, 115200);

// Registrar handlers antes de begin()
bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    UartHelloAckPayload ack{};
    bridge.sendHelloAck(ack);
});

// em loop():
bridge.loop();
```

Os métodos de envio dividem-se em dois grupos:

- ESP32 → RP2040: `sendHelloAck`, `sendCommand`, `sendProfileCommand`, `sendConfigPush`, `sendHeartbeat`, `sendClaimStatus`, `sendClaimComplete`, `sendWsStatus`, `sendTelemetryAck`, `sendCommandAck`, `sendConfigAck`.
- RP2040 → ESP32 (ou para testes): `sendHello`, `sendTelemetry`, `sendStatus`, `sendWeights`, `sendRfid`.

ACK/retry: quadros com a bandeira `UART_FLAG_ACK_REQ` são retentados até 3 vezes com um timeout de 700 ms. Se nenhum ACK for recebido — `send*` retorna `false`.

### Tipos de mensagem

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

Classes utilitárias para transferir grandes configs JSON sobre UART em fragmentos (cada fragmento ≤ 194 bytes de dados).

```cpp
// Receber (ESP32 ← RP2040)
ConfigReceiver rx;
bridge.setConfigChunkHandler([&rx, &mqtt](const UartConfigChunkPayload& p, uint8_t len, const UartFrameHeader& hdr) {
    if (rx.processFragment(p, len, hdr.flags) == ConfigFragResult::Complete) {
        mqtt.publishConfigRaw(rx.getJson(), rx.getLength());
        rx.reset();
    }
});

// Enviar (ESP32 → RP2040)
ConfigSender tx;
uint16_t tid = ConfigSender::generateTransferId();
tx.send(json, length, tid, [&](const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
    return bridge.sendConfigPushChunk(p, payloadLen, flags);
});
```

## Integração com CloudStateMachine

Dispositivos dual-MCU requerem o número de série de RP2040 antes do provisionamento:

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

A máquina de estados permanece em `WaitingForMcuSerial` até `setMcuSerial()` ser chamado.
