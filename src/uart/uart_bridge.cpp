#if defined(ESP32) || defined(ESP_PLATFORM)

#include "uart_bridge.h"
#include "../hal/hal_types.h"
#include <string.h>

namespace idryer {

namespace {
    constexpr uint8_t HEADER_SIZE = sizeof(UartFrameHeader);
}

// ============================================================================
// Init / loop
// ============================================================================

void UartBridge::begin(hal::ISerial* serial, uint32_t baudRate) {
    serial_   = serial;
    baudRate_ = baudRate;
    serial_->begin(baudRate_);
    reset();
}

void UartBridge::reset() {
    resetParser();
    pending_.active    = false;
    pending_.retries   = 0;
    pending_.lastSentAt = 0;
    seqCounter_        = 0;
}

void UartBridge::loop() {
    if (!serial_) return;
    while (serial_->available() > 0) {
        int b = serial_->read();
        if (b >= 0) processIncomingByte(static_cast<uint8_t>(b));
    }
    resendPendingIfNeeded();
}

// ============================================================================
// Transmit helpers
// ============================================================================

bool UartBridge::sendHello(const UartHelloPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Hello, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendHelloAck(const UartHelloAckPayload& p) {
    return transmit(UartMsgKind::HelloAck, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    UART_FLAG_IS_ACK);
}

bool UartBridge::sendTelemetry(const UartTelemetryPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Telemetry, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendStatus(const UartStatusPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Status, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendWeights(const UartWeightsPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Weights, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendRfid(const UartRfidPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Rfid, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendCommand(const UartCmdPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Command, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendProfileCommand(const UartProfilePayload& p, bool ackRequired) {
    return transmit(UartMsgKind::Command, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendConfigPush(const UartConfigPayload& p, bool ackRequired) {
    return transmit(UartMsgKind::ConfigPush, reinterpret_cast<const uint8_t*>(&p), sizeof(p),
                    ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendConfigPushChunk(const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags) {
    return transmit(UartMsgKind::ConfigPush, reinterpret_cast<const uint8_t*>(&p), payloadLen, flags);
}

bool UartBridge::sendHeartbeat(const UartHeartbeatPayload& p) {
    return transmit(UartMsgKind::Heartbeat, reinterpret_cast<const uint8_t*>(&p), sizeof(p), 0);
}

bool UartBridge::sendError(const UartErrorPayload& p) {
    return transmit(UartMsgKind::Error, reinterpret_cast<const uint8_t*>(&p), sizeof(p), UART_FLAG_ERROR);
}

bool UartBridge::sendTelemetryAck(uint8_t seq, UartErrCode status) {
    return sendAckFrame(UartMsgKind::TelemetryAck, seq, status);
}

bool UartBridge::sendCommandAck(uint8_t seq, UartErrCode status) {
    return sendAckFrame(UartMsgKind::CommandAck, seq, status);
}

bool UartBridge::sendConfigAck(uint8_t seq, UartErrCode status) {
    return sendAckFrame(UartMsgKind::ConfigAck, seq, status);
}

bool UartBridge::sendLog(const char* cstr) {
    if (!cstr) return false;
    size_t len = strlen(cstr);
    if (len > UART_MAX_PAYLOAD) len = UART_MAX_PAYLOAD;
    return transmit(UartMsgKind::Log, reinterpret_cast<const uint8_t*>(cstr), static_cast<uint8_t>(len), 0);
}

bool UartBridge::sendRfidWriteData(const UartRfidDataPayload& p, uint8_t flags) {
    return transmit(UartMsgKind::RfidWriteData, reinterpret_cast<const uint8_t*>(&p), sizeof(p), flags);
}

bool UartBridge::sendRfidReadData(const UartRfidDataPayload& p, uint8_t flags) {
    return transmit(UartMsgKind::RfidReadData, reinterpret_cast<const uint8_t*>(&p), sizeof(p), flags);
}

bool UartBridge::sendClaimStart(bool ackRequired) {
    return transmit(UartMsgKind::ClaimStart, nullptr, 0, ackRequired ? UART_FLAG_ACK_REQ : 0);
}

bool UartBridge::sendClaimStatus(const UartClaimStatusPayload& p) {
    return transmit(UartMsgKind::ClaimStatus, reinterpret_cast<const uint8_t*>(&p), sizeof(p), 0);
}

bool UartBridge::sendClaimComplete(const UartClaimCompletePayload& p) {
    return transmit(UartMsgKind::ClaimComplete, reinterpret_cast<const uint8_t*>(&p), sizeof(p), 0);
}

bool UartBridge::sendWsEnable(const UartWsEnablePayload& p) {
    return transmit(UartMsgKind::WsEnable, reinterpret_cast<const uint8_t*>(&p), sizeof(p), 0);
}

bool UartBridge::sendWsStatus(const UartWsStatusPayload& p) {
    return transmit(UartMsgKind::WsStatus, reinterpret_cast<const uint8_t*>(&p), sizeof(p), 0);
}

bool UartBridge::sendWsResetClients() {
    return transmit(UartMsgKind::WsResetClients, nullptr, 0, 0);
}

bool UartBridge::sendWsStatusRequest() {
    return transmit(UartMsgKind::WsStatusRequest, nullptr, 0, 0);
}

bool UartBridge::waitForAck(uint32_t timeoutMs) {
    const uint32_t start = HAL_MILLIS();
    while (pending_.active && (HAL_MILLIS() - start) < timeoutMs) {
        loop();
        HAL_DELAY_MS(1);
    }
    return !pending_.active;
}

// ============================================================================
// Parser
// ============================================================================

void UartBridge::processIncomingByte(uint8_t byte) {
    switch (parser_.state) {
    case ParserState::WaitForSof:
        if (byte == UART_SOF) {
            parser_.frame.header.sof = byte;
            parser_.headerIdx = 1;
            parser_.state = ParserState::Header;
        }
        break;

    case ParserState::Header: {
        uint8_t* raw = reinterpret_cast<uint8_t*>(&parser_.frame.header);
        raw[parser_.headerIdx++] = byte;
        if (parser_.headerIdx >= HEADER_SIZE) {
            if (parser_.frame.header.payloadLength > UART_MAX_PAYLOAD) {
                emitError(UartErrCode::InvalidPayload, parser_.frame.header.sequence,
                          parser_.frame.header.payloadLength, false);
                resetParser();
                break;
            }
            parser_.payloadIdx = 0;
            parser_.state = (parser_.frame.header.payloadLength > 0)
                            ? ParserState::Payload : ParserState::Crc;
        }
        break;
    }

    case ParserState::Payload:
        parser_.frame.payload[parser_.payloadIdx++] = byte;
        if (parser_.payloadIdx >= parser_.frame.header.payloadLength) {
            parser_.state  = ParserState::Crc;
            parser_.crcIdx = 0;
        }
        break;

    case ParserState::Crc:
        if (parser_.crcIdx == 0) {
            parser_.frame.crc = byte;
            parser_.crcIdx    = 1;
        } else {
            parser_.frame.crc |= static_cast<uint16_t>(byte) << 8;
            UartFrame frame    = parser_.frame;
            resetParser();

            uint8_t buf[HEADER_SIZE + UART_MAX_PAYLOAD];
            memcpy(buf, &frame.header, HEADER_SIZE);
            if (frame.header.payloadLength > 0)
                memcpy(buf + HEADER_SIZE, frame.payload, frame.header.payloadLength);

            uint16_t computed = uartCalculateCrc(buf, HEADER_SIZE + frame.header.payloadLength);
            if (computed != frame.crc) {
                HAL_LOG_ERROR("UART", "CRC mismatch seq=%u kind=0x%02X exp=0x%04X got=0x%04X",
                              frame.header.sequence,
                              static_cast<uint8_t>(frame.header.kind),
                              computed, frame.crc);
                emitError(UartErrCode::CrcMismatch, frame.header.sequence,
                          frame.header.payloadLength, false);
                break;
            }
            handleFrame(frame);
        }
        break;
    }
}

// ============================================================================
// Frame dispatch
// ============================================================================

void UartBridge::handleFrame(const UartFrame& frame) {
    if (frame.header.version != UART_PROTOCOL_VER) {
        emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                  frame.header.version, false);
        return;
    }

    switch (frame.header.kind) {
    case UartMsgKind::Telemetry: {
        if (!validateLength(UartMsgKind::Telemetry, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (telemetryHandler_) {
            UartTelemetryPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            telemetryHandler_(p, frame.header);
        }
        if (frame.header.flags & UART_FLAG_ACK_REQ) sendTelemetryAck(frame.header.sequence);
        break;
    }

    case UartMsgKind::Weights: {
        if (!validateLength(UartMsgKind::Weights, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (weightsHandler_) {
            UartWeightsPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            weightsHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::Status: {
        if (!validateLength(UartMsgKind::Status, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (statusHandler_) {
            UartStatusPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            statusHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::Rfid: {
        if (!validateLength(UartMsgKind::Rfid, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (rfidHandler_) {
            UartRfidPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            rfidHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::RfidReadData:
    case UartMsgKind::RfidWriteData: {
        if (!validateLength(frame.header.kind, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (rfidDataHandler_) {
            UartRfidDataPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            rfidDataHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::Hello: {
        if (!validateLength(UartMsgKind::Hello, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (helloHandler_) {
            UartHelloPayload p{}; memcpy(&p, frame.payload, frame.header.payloadLength);
            helloHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::HelloAck: {
        if (!validateLength(UartMsgKind::HelloAck, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (helloAckHandler_) {
            UartHelloAckPayload p{}; memcpy(&p, frame.payload, frame.header.payloadLength);
            helloAckHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::TelemetryAck:
    case UartMsgKind::CommandAck:
    case UartMsgKind::ConfigAck:
        handleAckFrame(frame);
        break;

    case UartMsgKind::Command: {
        if (!validateLength(UartMsgKind::Command, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (frame.header.payloadLength == sizeof(UartProfilePayload)) {
            if (profileHandler_) {
                UartProfilePayload p{}; memcpy(&p, frame.payload, sizeof(p));
                profileHandler_(p, frame.header);
            }
        } else {
            if (commandHandler_) {
                UartCmdPayload p{}; memcpy(&p, frame.payload, sizeof(p));
                commandHandler_(p, frame.header);
            }
        }
        break;
    }

    case UartMsgKind::ConfigPush: {
        if (frame.header.flags & (UART_FLAG_FRAGMENT | UART_FLAG_LAST_FRAGMENT)) {
            if (frame.header.payloadLength < UART_CONFIG_CHUNK_HEADER_SIZE) {
                emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false); return;
            }
            if (configChunkHandler_) {
                UartConfigChunkPayload p{};
                memcpy(&p, frame.payload, frame.header.payloadLength);
                uint8_t dataLen = frame.header.payloadLength - UART_CONFIG_CHUNK_HEADER_SIZE;
                configChunkHandler_(p, dataLen, frame.header);
            }
            if (frame.header.flags & UART_FLAG_ACK_REQ) sendConfigAck(frame.header.sequence);
        } else {
            if (!validateLength(UartMsgKind::ConfigPush, frame.header.payloadLength)) {
                emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                          frame.header.payloadLength, false); return;
            }
            if (configHandler_) {
                UartConfigPayload p{}; memcpy(&p, frame.payload, sizeof(p));
                configHandler_(p, frame.header);
            }
        }
        break;
    }

    case UartMsgKind::Heartbeat: {
        if (!validateLength(UartMsgKind::Heartbeat, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (heartbeatHandler_) {
            UartHeartbeatPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            heartbeatHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::Error: {
        if (!validateLength(UartMsgKind::Error, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        UartErrorPayload p{}; memcpy(&p, frame.payload, sizeof(p));
        emitError(p.code, p.lastSequence, p.detail, true);
        break;
    }

    case UartMsgKind::Log:
        if (logHandler_) logHandler_(frame.payload, frame.header.payloadLength);
        break;

    case UartMsgKind::ClaimStart:
        if (claimStartHandler_) claimStartHandler_(frame.header);
        break;

    case UartMsgKind::ClaimStatus: {
        if (!validateLength(UartMsgKind::ClaimStatus, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (claimStatusHandler_) {
            UartClaimStatusPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            claimStatusHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::ClaimComplete: {
        if (!validateLength(UartMsgKind::ClaimComplete, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (claimCompleteHandler_) {
            UartClaimCompletePayload p{}; memcpy(&p, frame.payload, sizeof(p));
            claimCompleteHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::WsEnable: {
        if (!validateLength(UartMsgKind::WsEnable, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (wsEnableHandler_) {
            UartWsEnablePayload p{}; memcpy(&p, frame.payload, sizeof(p));
            wsEnableHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::WsStatus: {
        if (!validateLength(UartMsgKind::WsStatus, frame.header.payloadLength)) {
            emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                      frame.header.payloadLength, false); return;
        }
        if (wsStatusHandler_) {
            UartWsStatusPayload p{}; memcpy(&p, frame.payload, sizeof(p));
            wsStatusHandler_(p, frame.header);
        }
        break;
    }

    case UartMsgKind::WsResetClients:
        if (wsResetClientsHandler_) wsResetClientsHandler_(frame.header);
        break;

    case UartMsgKind::WsStatusRequest:
        if (wsStatusRequestHandler_) wsStatusRequestHandler_(frame.header);
        break;

    default:
        emitError(UartErrCode::UnknownMessage, frame.header.sequence,
                  static_cast<uint16_t>(frame.header.kind), false);
        break;
    }
}

// ============================================================================
// ACK handling
// ============================================================================

void UartBridge::handleAckFrame(const UartFrame& frame) {
    if (!validateLength(frame.header.kind, frame.header.payloadLength)) {
        emitError(UartErrCode::InvalidPayload, frame.header.sequence,
                  frame.header.payloadLength, false); return;
    }
    UartAckPayload p{}; memcpy(&p, frame.payload, sizeof(p));

    if (pending_.active && pending_.frame.header.sequence == p.ackSequence)
        pending_.active = false;

    switch (frame.header.kind) {
    case UartMsgKind::CommandAck:
        if (commandAckHandler_) commandAckHandler_(p, frame.header);
        break;
    case UartMsgKind::ConfigAck:
        if (configAckHandler_) configAckHandler_(p, frame.header);
        break;
    default: break;
    }
}

// ============================================================================
// Transmit
// ============================================================================

bool UartBridge::transmit(UartMsgKind kind, const uint8_t* payload,
                          uint8_t length, uint8_t flags, int forcedSeq) {
    if (!serial_ || length > UART_MAX_PAYLOAD) return false;

    UartFrame frame{};
    frame.header.sof           = UART_SOF;
    frame.header.version       = UART_PROTOCOL_VER;
    frame.header.flags         = flags;
    frame.header.kind          = kind;
    frame.header.sequence      = (forcedSeq >= 0) ? static_cast<uint8_t>(forcedSeq) : nextSequence();
    frame.header.payloadLength = length;

    if (length > 0 && payload)
        memcpy(frame.payload, payload, length);

    uint8_t buf[HEADER_SIZE + UART_MAX_PAYLOAD];
    memcpy(buf, &frame.header, HEADER_SIZE);
    if (length > 0 && payload)
        memcpy(buf + HEADER_SIZE, frame.payload, length);
    frame.crc = uartCalculateCrc(buf, HEADER_SIZE + length);

    HAL_LOG_DEBUG("UART", "TX seq=%u kind=0x%02X len=%u",
                  frame.header.sequence, static_cast<uint8_t>(kind), length);

    size_t written = serial_->write(reinterpret_cast<uint8_t*>(&frame.header), HEADER_SIZE);
    if (length > 0) written += serial_->write(frame.payload, length);
    uint8_t crcBytes[2] = { static_cast<uint8_t>(frame.crc & 0xFF),
                             static_cast<uint8_t>((frame.crc >> 8) & 0xFF) };
    written += serial_->write(crcBytes, 2);
    serial_->flush();
    HAL_DELAY_MS(2);

    if (written != HEADER_SIZE + length + 2u) {
        HAL_LOG_ERROR("UART", "TX incomplete seq=%u written=%u expected=%u",
                      frame.header.sequence, written, HEADER_SIZE + length + 2u);
        return false;
    }

    if (flags & UART_FLAG_ACK_REQ) {
        pending_.frame      = frame;
        pending_.active     = true;
        pending_.retries    = 0;
        pending_.lastSentAt = HAL_MILLIS();
    }
    return true;
}

bool UartBridge::sendAckFrame(UartMsgKind kind, uint8_t sequence, UartErrCode status) {
    UartAckPayload p{};
    p.ackSequence = sequence;
    p.status      = status;
    return transmit(kind, reinterpret_cast<const uint8_t*>(&p), sizeof(p), UART_FLAG_IS_ACK);
}

void UartBridge::resendPendingIfNeeded() {
    if (!pending_.active || !serial_) return;
    if (HAL_MILLIS() - pending_.lastSentAt < UART_CMD_TIMEOUT_MS) return;

    if (pending_.retries + 1 >= UART_MAX_RETRIES) {
        pending_.active = false;
        emitError(UartErrCode::Timeout, pending_.frame.header.sequence, pending_.retries, false);
        return;
    }

    pending_.retries++;
    pending_.lastSentAt = HAL_MILLIS();

    size_t written = serial_->write(reinterpret_cast<uint8_t*>(&pending_.frame.header), HEADER_SIZE);
    if (pending_.frame.header.payloadLength > 0)
        written += serial_->write(pending_.frame.payload, pending_.frame.header.payloadLength);
    uint8_t crcBytes[2] = { static_cast<uint8_t>(pending_.frame.crc & 0xFF),
                             static_cast<uint8_t>((pending_.frame.crc >> 8) & 0xFF) };
    written += serial_->write(crcBytes, 2);
    serial_->flush();

    HAL_LOG_DEBUG("UART", "Retry seq=%u attempt=%u", pending_.frame.header.sequence, pending_.retries);
}

// ============================================================================
// Validation / helpers
// ============================================================================

bool UartBridge::validateLength(UartMsgKind kind, uint8_t length) const {
    switch (kind) {
    case UartMsgKind::Hello:          return length == sizeof(UartHelloPayload);
    case UartMsgKind::HelloAck:       return length == sizeof(UartHelloAckPayload);
    case UartMsgKind::Telemetry:      return length == sizeof(UartTelemetryPayload);
    case UartMsgKind::Weights:        return length == sizeof(UartWeightsPayload);
    case UartMsgKind::Status:         return length == sizeof(UartStatusPayload);
    case UartMsgKind::Rfid:           return length == sizeof(UartRfidPayload);
    case UartMsgKind::RfidReadData:
    case UartMsgKind::RfidWriteData:  return length == sizeof(UartRfidDataPayload);
    case UartMsgKind::Command:        return length == sizeof(UartCmdPayload) ||
                                             length == sizeof(UartProfilePayload);
    case UartMsgKind::ConfigPush:     return length == sizeof(UartConfigPayload);
    case UartMsgKind::Heartbeat:      return length == sizeof(UartHeartbeatPayload);
    case UartMsgKind::TelemetryAck:
    case UartMsgKind::CommandAck:
    case UartMsgKind::ConfigAck:      return length == sizeof(UartAckPayload);
    case UartMsgKind::Error:          return length == sizeof(UartErrorPayload);
    case UartMsgKind::Log:            return length <= UART_MAX_PAYLOAD;
    case UartMsgKind::ClaimStart:     return length == 0;
    case UartMsgKind::ClaimStatus:    return length == sizeof(UartClaimStatusPayload);
    case UartMsgKind::ClaimComplete:  return length == sizeof(UartClaimCompletePayload);
    case UartMsgKind::WsEnable:       return length == sizeof(UartWsEnablePayload);
    case UartMsgKind::WsStatus:       return length == sizeof(UartWsStatusPayload);
    case UartMsgKind::WsResetClients:
    case UartMsgKind::WsStatusRequest: return length == 0;
    default:                          return length <= UART_MAX_PAYLOAD;
    }
}

void UartBridge::emitError(UartErrCode code, uint8_t sequence, uint16_t detail, bool remote) {
    UartErrorPayload p{};
    p.code         = code;
    p.lastSequence = sequence;
    p.detail       = detail;
    if (!remote) sendError(p);
    if (errorHandler_) errorHandler_(p, remote);
}

void UartBridge::resetParser() {
    parser_.state      = ParserState::WaitForSof;
    parser_.headerIdx  = 0;
    parser_.payloadIdx = 0;
    parser_.crcIdx     = 0;
}

uint8_t UartBridge::nextSequence() { return seqCounter_++; }

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
