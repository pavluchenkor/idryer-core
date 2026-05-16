# UART-Schicht

Das UART-Modul implementiert ein binäres Frame-Protokoll für bidirektionale Kommunikation zwischen ESP32 (Link) und RP2040 (Controller). Wird in Dual-MCU-Geräten verwendet: iDryer LINK, iHeater LINK.

**Storage Link verwendet das UART-Modul nicht** — es ist ein eigenständiges ESP32-C3-Gerät ohne zweiten MCU.

Separat einfügen:

```cpp
#include <idryer_uart.h>
```

## Physikalische Schicht

- UART 8N1, 115200 Baud (Standard), keine Hardware-Flusskontrolle.
- Maximale Frame-Payload: 200 Bytes.
- CRC-16/CCITT (poly 0x1021, init 0xFFFF) pro Frame.

## Frame-Struktur

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

## Klassen

### UartBridge

Hauptklasse des Moduls. Verarbeitet den eingehenden Stream Byte für Byte, erstellt Frames, überprüft CRC, verwaltet ACK/Retry und sendet Frames an registrierte Callbacks.

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

Send-Methoden sind in zwei Gruppen aufgeteilt:

- ESP32 → RP2040: `sendHelloAck`, `sendCommand`, `sendProfileCommand`, `sendConfigPush`, `sendHeartbeat`, `sendClaimStatus`, `sendClaimComplete`, `sendWsStatus`, `sendTelemetryAck`, `sendCommandAck`, `sendConfigAck`.
- RP2040 → ESP32 (oder für Tests): `sendHello`, `sendTelemetry`, `sendStatus`, `sendWeights`, `sendRfid`.

ACK/Retry: Frames mit dem `UART_FLAG_ACK_REQ` Flag werden bis zu 3 Mal mit einem 700-ms-Timeout wiederholt. Wenn keine ACK empfangen wird — `send*` gibt `false` zurück.

### Nachrichtentypen

| Typ | Code | Richtung | Zweck |
|------|------|-----------|---------|
| `Hello` | 0x01 | RP2040 → ESP32 | Ankündigung beim Start; enthält MCU-Seriennummer, Gerätetyp, Fähigkeiten |
| `HelloAck` | 0x02 | ESP32 → RP2040 | Antwort mit IP-Adresse und SSID |
| `Telemetry` | 0x10 | RP2040 → ESP32 | Temperatur, Luftfeuchtigkeit, Heizungsleistung |
| `Weights` | 0x12 | RP2040 → ESP32 | Wagenlesungen |
| `Status` | 0x13 | RP2040 → ESP32 | Aktueller Trocknungsmodus, Sitzungsfortschritt |
| `Rfid` | 0x14 | RP2040 → ESP32 | RFID-Ereignis (Tag erkannt/entfernt) |
| `Command` | 0x20 | ESP32 → RP2040 | Befehl vom Backend (Start, Stop, Find...) |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | Konfiguration (einfach oder fragmentiert) |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | Betriebszeit, RSSI, Cloud-Status |
| `Error` | 0x50 | beide | Protokollfehler |
| `ClaimStart..Complete` | 0x70–0x72 | beide | Claiming-Lebenszyklus |
| `WsEnable..StatusRequest` | 0x73–0x76 | beide | WebSocket-Server-Steuerung auf RP2040 |

### ConfigReceiver / ConfigSender

Hilfsfunktions-Klassen für die Übertragung großer JSON-Konfigurationen über UART in Fragmenten (jedes Fragment ≤ 194 Bytes Daten).

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

## Integration mit CloudStateMachine

Dual-MCU-Geräte benötigen die Seriennummer von RP2040 vor dem Provisioning:

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

Die State Machine bleibt in `WaitingForMcuSerial` bis `setMcuSerial()` aufgerufen wird.
