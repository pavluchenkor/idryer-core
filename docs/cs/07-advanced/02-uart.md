# Vrstva UART

Modul UART implementuje binární rámcový protokol pro obousměrnou komunikaci mezi ESP32 (Link) a RP2040 (Controller). Používá se v zařízeních se dvěma MCU: iDryer LINK, iHeater LINK.

**Storage Link nepoužívá modul UART** — jedná se o samostatné zařízení ESP32-C3 bez druhého MCU.

Zahrňte samostatně:

```cpp
#include <idryer_uart.h>
```

## Fyzická vrstva

- UART 8N1, 115200 baudů (výchozí), bez hardwarového řízení toku.
- Maximální datová část rámce: 200 bajtů.
- CRC-16/CCITT (poly 0x1021, init 0xFFFF) na rámec.

## Struktura rámce

```
bajt 0  : SOF = 0xAA
bajt 1  : verze = 1
bajt 2  : příznaky (je vyžadován ACK | je ACK | chyba | fragment | poslední fragment)
bajt 3  : typ zprávy (UartMsgKind)
bajt 4  : číslo sekvence (0..255, wrap)
bajt 5  : délka datové části
datová část : data (0..200 bajtů)
crc16   : CRC, nízký bajt + vysoký bajt
```

## Třídy

### UartBridge

Hlavní třída modulu. Zpracovává příchozí tok bajt po bajtu, vytváří rámce, ověřuje CRC, spravuje ACK/retry a odesílá rámce do registrovaných zpětných volání.

```cpp
UartBridge bridge;
bridge.begin(&Serial1, 115200);

// Zaregistrujte obslužné programy před begin()
bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    UartHelloAckPayload ack{};
    bridge.sendHelloAck(ack);
});

// v loop():
bridge.loop();
```

Metody odesílání jsou rozděleny do dvou skupin:

- ESP32 → RP2040: `sendHelloAck`, `sendCommand`, `sendProfileCommand`, `sendConfigPush`, `sendHeartbeat`, `sendClaimStatus`, `sendClaimComplete`, `sendWsStatus`, `sendTelemetryAck`, `sendCommandAck`, `sendConfigAck`.
- RP2040 → ESP32 (nebo pro testy): `sendHello`, `sendTelemetry`, `sendStatus`, `sendWeights`, `sendRfid`.

ACK/retry: rámce s příznakem `UART_FLAG_ACK_REQ` se opakují až 3krát s vypršením časového limitu 700 ms. Pokud je přijat ACK — `send*` vrátí `false`.

### Typy zpráv

| Typ | Kód | Směr | Účel |
|------|------|-----------|---------|
| `Hello` | 0x01 | RP2040 → ESP32 | Oznámení při spuštění; obsahuje sériové číslo MCU, typ zařízení, schopnosti |
| `HelloAck` | 0x02 | ESP32 → RP2040 | Odpověď s IP adresou a SSID |
| `Telemetry` | 0x10 | RP2040 → ESP32 | Teplota, vlhkost, výkon topidla |
| `Weights` | 0x12 | RP2040 → ESP32 | Čtení váhy |
| `Status` | 0x13 | RP2040 → ESP32 | Aktuální režim sušení, průběh relace |
| `Rfid` | 0x14 | RP2040 → ESP32 | Událost RFID (tag detekován/odebrán) |
| `Command` | 0x20 | ESP32 → RP2040 | Příkaz z backendu (start, stop, find...) |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | Konfigurace (jednoduchá nebo dělená) |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | Provozní doba, RSSI, stav cloudu |
| `Error` | 0x50 | oba | Chyba protokolu |
| `ClaimStart..Complete` | 0x70–0x72 | oba | Životní cyklus žádosti |
| `WsEnable..StatusRequest` | 0x73–0x76 | oba | Řízení WebSocket serveru na RP2040 |

### ConfigReceiver / ConfigSender

Pomocné třídy pro přenos velkých konfigurací JSON přes UART v fragmentech (každý fragment ≤ 194 bajtů dat).

```cpp
// Přijmout (ESP32 ← RP2040)
ConfigReceiver rx;
bridge.setConfigChunkHandler([&rx, &mqtt](const UartConfigChunkPayload& p, uint8_t len, const UartFrameHeader& hdr) {
    if (rx.processFragment(p, len, hdr.flags) == ConfigFragResult::Complete) {
        mqtt.publishConfigRaw(rx.getJson(), rx.getLength());
        rx.reset();
    }
});

// Poslat (ESP32 → RP2040)
ConfigSender tx;
uint16_t tid = ConfigSender::generateTransferId();
tx.send(json, length, tid, [&](const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
    return bridge.sendConfigPushChunk(p, payloadLen, flags);
});
```

## Integrace s CloudStateMachine

Zařízení se dvěma MCU vyžadují sériové číslo z RP2040 před zřizováním:

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

Stavový stroj zůstane v `WaitingForMcuSerial` až do zavolání `setMcuSerial()`.
