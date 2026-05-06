# UART-слой

UART-модуль реализует бинарный фреймовый протокол для двунаправленного обмена между ESP32 (Link) и RP2040 (Controller). Используется в двухпроцессорных устройствах: iDryer LINK, iHeater LINK.

**Storage Link не использует UART-модуль** — это standalone ESP32-C3 устройство без второго MCU.

Подключается отдельно:

```cpp
#include <idryer_uart.h>
```

## Физический уровень

- UART 8N1, 115200 бод (по умолчанию), нет аппаратного flow control.
- Максимальный payload кадра: 200 байт.
- CRC-16/CCITT (poly 0x1021, init 0xFFFF) для каждого кадра.

## Структура кадра

```
byte 0  : SOF = 0xAA
byte 1  : version = 1
byte 2  : flags   (ACK required | is ACK | error | fragment | last fragment)
byte 3  : message kind (UartMsgKind)
byte 4  : sequence number (0..255, wrap)
byte 5  : payload length
payload : данные (0..200 байт)
crc16   : CRC, low byte + high byte
```

## Классы

### UartBridge

Основной класс модуля. Обрабатывает входящий поток побайтово, строит кадры, проверяет CRC, управляет ACK/retry и диспатчит кадры на зарегистрированные колбэки.

```cpp
UartBridge bridge;
bridge.begin(&Serial1, 115200);

// Назначить обработчики до begin()
bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    UartHelloAckPayload ack{};
    bridge.sendHelloAck(ack);
});

// в loop():
bridge.loop();
```

Методы отправки делятся на две группы:

- ESP32 → RP2040: `sendHelloAck`, `sendCommand`, `sendProfileCommand`, `sendConfigPush`, `sendHeartbeat`, `sendClaimStatus`, `sendClaimComplete`, `sendWsStatus`, `sendTelemetryAck`, `sendCommandAck`, `sendConfigAck`.
- RP2040 → ESP32 (или для тестов): `sendHello`, `sendTelemetry`, `sendStatus`, `sendWeights`, `sendRfid`.

ACK/retry: кадры с флагом `UART_FLAG_ACK_REQ` повторяются до 3 раз с таймаутом 700 мс. Если ACK не получен — `send*` возвращает `false`.

### Типы сообщений

| Kind | Код | Направление | Назначение |
|------|-----|-------------|------------|
| `Hello` | 0x01 | RP2040 → ESP32 | Анонс при старте; содержит серийник MCU, тип устройства, capabilities |
| `HelloAck` | 0x02 | ESP32 → RP2040 | Ответ с IP-адресом и SSID |
| `Telemetry` | 0x10 | RP2040 → ESP32 | Температура, влажность, мощность нагревателя |
| `Weights` | 0x12 | RP2040 → ESP32 | Показания весов |
| `Status` | 0x13 | RP2040 → ESP32 | Текущий режим сушки, прогресс сессии |
| `Rfid` | 0x14 | RP2040 → ESP32 | RFID-событие (тег обнаружен/убран) |
| `Command` | 0x20 | ESP32 → RP2040 | Команда из бэкенда (start, stop, find...) |
| `ConfigPush` | 0x30 | ESP32 → RP2040 | Конфигурация (простая или чанкованная) |
| `Heartbeat` | 0x40 | ESP32 → RP2040 | Uptime, RSSI, состояние облака |
| `Error` | 0x50 | оба | Ошибка протокола |
| `ClaimStart..Complete` | 0x70–0x72 | оба | Жизненный цикл claiming |
| `WsEnable..StatusRequest` | 0x73–0x76 | оба | Управление WebSocket-сервером на RP2040 |

### ConfigReceiver / ConfigSender

Утилитарные классы для передачи больших JSON-конфигов через UART фрагментами (каждый фрагмент ≤ 194 байта данных).

```cpp
// Приём (ESP32 ← RP2040)
ConfigReceiver rx;
bridge.setConfigChunkHandler([&rx, &mqtt](const UartConfigChunkPayload& p, uint8_t len, const UartFrameHeader& hdr) {
    if (rx.processFragment(p, len, hdr.flags) == ConfigFragResult::Complete) {
        mqtt.publishConfigRaw(rx.getJson(), rx.getLength());
        rx.reset();
    }
});

// Отправка (ESP32 → RP2040)
ConfigSender tx;
uint16_t tid = ConfigSender::generateTransferId();
tx.send(json, length, tid, [&](const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
    return bridge.sendConfigPushChunk(p, payloadLen, flags);
});
```

## Интеграция с CloudStateMachine

Двухпроцессорные устройства требуют передачи серийника от RP2040 перед provisioning:

```cpp
idryer::cloud::CloudConfig cfg;
cfg.waitForMcuSerial = true;
idryer::cloud::CloudStateMachine cloud(..., cfg);

bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
    cloud.setMcuSerial(p.mcuSerial);
    // ...
});
```

Стейт-машина находится в `WaitingForMcuSerial` пока `setMcuSerial()` не вызван.
