#pragma once

#include <functional>
#include "../hal/hal_types.h"
#include "uart_protocol.h"

namespace idryer {

/**
 * @brief Bidirectional UART transport between ESP32 (Link) and RP2040 (Controller).
 *
 * Handles frame parsing, CRC validation, ACK/retry logic, and dispatches
 * received messages to registered callbacks.
 *
 * Usage — in your product assembly:
 * @code
 * UartBridge bridge;
 * bridge.begin(&Serial1, 115200);
 *
 * bridge.setHelloHandler([&](const UartHelloPayload& p, const UartFrameHeader&) {
 *     cloud.setMcuSerial(p.mcuSerial);
 *     // send back IP/SSID
 *     UartHelloAckPayload ack{};
 *     // fill ack...
 *     bridge.sendHelloAck(ack);
 * });
 *
 * // in loop():
 * bridge.loop();
 * @endcode
 *
 * All @c send*() methods return @c false if the serial port isn't initialized
 * or if an ACK-required frame isn't acknowledged after @c UART_MAX_RETRIES.
 */
class UartBridge {
public:
    using HelloHandler           = std::function<void(const UartHelloPayload&,    const UartFrameHeader&)>;
    using HelloAckHandler        = std::function<void(const UartHelloAckPayload&, const UartFrameHeader&)>;
    using TelemetryHandler       = std::function<void(const UartTelemetryPayload&, const UartFrameHeader&)>;
    using CommandHandler         = std::function<void(const UartCmdPayload&,       const UartFrameHeader&)>;
    using ProfileHandler         = std::function<void(const UartProfilePayload&,   const UartFrameHeader&)>;
    using ConfigHandler          = std::function<void(const UartConfigPayload&,    const UartFrameHeader&)>;
    using ConfigChunkHandler     = std::function<void(const UartConfigChunkPayload&, uint8_t dataLen, const UartFrameHeader&)>;
    using CommandAckHandler      = std::function<void(const UartAckPayload&,       const UartFrameHeader&)>;
    using ConfigAckHandler       = std::function<void(const UartAckPayload&,       const UartFrameHeader&)>;
    using HeartbeatHandler       = std::function<void(const UartHeartbeatPayload&, const UartFrameHeader&)>;
    using ErrorHandler           = std::function<void(const UartErrorPayload&,     bool remote)>;
    using LogHandler             = std::function<void(const uint8_t* payload,      uint8_t length)>;
    using WeightsHandler         = std::function<void(const UartWeightsPayload&,   const UartFrameHeader&)>;
    using StatusHandler          = std::function<void(const UartStatusPayload&,    const UartFrameHeader&)>;
    using RfidHandler            = std::function<void(const UartRfidPayload&,      const UartFrameHeader&)>;
    using RfidDataHandler        = std::function<void(const UartRfidDataPayload&,  const UartFrameHeader&)>;
    using ClaimStartHandler      = std::function<void(const UartFrameHeader&)>;
    using ClaimStatusHandler     = std::function<void(const UartClaimStatusPayload&,   const UartFrameHeader&)>;
    using ClaimCompleteHandler   = std::function<void(const UartClaimCompletePayload&,  const UartFrameHeader&)>;
    using WsEnableHandler        = std::function<void(const UartWsEnablePayload&,  const UartFrameHeader&)>;
    using WsStatusHandler        = std::function<void(const UartWsStatusPayload&,  const UartFrameHeader&)>;
    using WsResetClientsHandler  = std::function<void(const UartFrameHeader&)>;
    using WsStatusRequestHandler = std::function<void(const UartFrameHeader&)>;

    /**
     * @brief Initializes the bridge on the given serial port.
     * @param serial   Platform serial port (e.g. @c &Serial1).
     * @param baudRate Baud rate. Must match the RP2040 side. Default: 115200.
     */
    void begin(hal::ISerial* serial, uint32_t baudRate = 115200);

    /**
     * @brief Processes incoming bytes and retransmits pending ACK-required frames.
     *
     * Call every iteration of the main loop.
     */
    void loop();

    /// @brief Resets the frame parser and clears any pending retry state.
    void reset();

    /// @name ESP32 → RP2040 transmit methods
    /// @{
    bool sendHelloAck(const UartHelloAckPayload& p);
    bool sendCommand(const UartCmdPayload& p, bool ackRequired = true);
    bool sendProfileCommand(const UartProfilePayload& p, bool ackRequired = true);
    bool sendConfigPush(const UartConfigPayload& p, bool ackRequired = true);
    bool sendConfigPushChunk(const UartConfigChunkPayload& p, uint8_t payloadLen, uint8_t flags);
    bool sendHeartbeat(const UartHeartbeatPayload& p);
    bool sendClaimStatus(const UartClaimStatusPayload& p);
    bool sendClaimComplete(const UartClaimCompletePayload& p);
    bool sendWsStatus(const UartWsStatusPayload& p);
    bool sendTelemetryAck(uint8_t sequence, UartErrCode status = UartErrCode::None);
    bool sendCommandAck(uint8_t sequence, UartErrCode status = UartErrCode::None);
    bool sendConfigAck(uint8_t sequence, UartErrCode status = UartErrCode::None);
    bool sendError(const UartErrorPayload& p);
    bool sendLog(const char* cstr);
    bool sendRfidWriteData(const UartRfidDataPayload& p, uint8_t flags = 0);
    /// @}

    /// @name RP2040-side methods (also usable in tests)
    /// @{
    bool sendHello(const UartHelloPayload& p, bool ackRequired = false);
    bool sendTelemetry(const UartTelemetryPayload& p, bool ackRequired = false);
    bool sendStatus(const UartStatusPayload& p, bool ackRequired = false);
    bool sendWeights(const UartWeightsPayload& p, bool ackRequired = false);
    bool sendRfid(const UartRfidPayload& p, bool ackRequired = false);
    bool sendRfidReadData(const UartRfidDataPayload& p, uint8_t flags = 0);
    bool sendClaimStart(bool ackRequired = true);
    bool sendWsEnable(const UartWsEnablePayload& p);
    bool sendWsResetClients();
    bool sendWsStatusRequest();
    /// @}

    /**
     * @brief Blocks until an ACK is received or @p timeoutMs elapses.
     * @return @c true if an ACK arrived in time.
     */
    bool waitForAck(uint32_t timeoutMs);

    /// @name Message handlers — register before calling begin()
    /// @{
    void setHelloHandler(const HelloHandler& h)                  { helloHandler_ = h; }
    void setHelloAckHandler(const HelloAckHandler& h)            { helloAckHandler_ = h; }
    void setTelemetryHandler(const TelemetryHandler& h)          { telemetryHandler_ = h; }
    void setCommandHandler(const CommandHandler& h)              { commandHandler_ = h; }
    void setProfileHandler(const ProfileHandler& h)              { profileHandler_ = h; }
    void setConfigHandler(const ConfigHandler& h)                { configHandler_ = h; }
    void setConfigChunkHandler(const ConfigChunkHandler& h)      { configChunkHandler_ = h; }
    void setCommandAckHandler(const CommandAckHandler& h)        { commandAckHandler_ = h; }
    void setConfigAckHandler(const ConfigAckHandler& h)          { configAckHandler_ = h; }
    void setHeartbeatHandler(const HeartbeatHandler& h)          { heartbeatHandler_ = h; }
    void setErrorHandler(const ErrorHandler& h)                  { errorHandler_ = h; }
    void setLogHandler(const LogHandler& h)                      { logHandler_ = h; }
    void setWeightsHandler(const WeightsHandler& h)              { weightsHandler_ = h; }
    void setStatusHandler(const StatusHandler& h)                { statusHandler_ = h; }
    void setRfidHandler(const RfidHandler& h)                    { rfidHandler_ = h; }
    void setRfidDataHandler(const RfidDataHandler& h)            { rfidDataHandler_ = h; }
    void setClaimStartHandler(const ClaimStartHandler& h)        { claimStartHandler_ = h; }
    void setClaimStatusHandler(const ClaimStatusHandler& h)      { claimStatusHandler_ = h; }
    void setClaimCompleteHandler(const ClaimCompleteHandler& h)  { claimCompleteHandler_ = h; }
    void setWsEnableHandler(const WsEnableHandler& h)            { wsEnableHandler_ = h; }
    void setWsStatusHandler(const WsStatusHandler& h)            { wsStatusHandler_ = h; }
    void setWsResetClientsHandler(const WsResetClientsHandler& h){ wsResetClientsHandler_ = h; }
    void setWsStatusRequestHandler(const WsStatusRequestHandler& h){ wsStatusRequestHandler_ = h; }
    /// @}

private:
    enum class ParserState : uint8_t { WaitForSof, Header, Payload, Crc };

    struct ParserCtx {
        ParserState  state = ParserState::WaitForSof;
        UartFrame    frame{};
        uint8_t      headerIdx  = 0;
        uint8_t      payloadIdx = 0;
        uint8_t      crcIdx     = 0;
    };

    struct PendingFrame {
        UartFrame frame{};
        bool      active    = false;
        uint8_t   retries   = 0;
        uint32_t  lastSentAt = 0;
    };

    void processIncomingByte(uint8_t byte);
    void handleFrame(const UartFrame& frame);
    void handleAckFrame(const UartFrame& frame);
    bool transmit(UartMsgKind kind, const uint8_t* payload, uint8_t length, uint8_t flags, int forcedSeq = -1);
    bool sendAckFrame(UartMsgKind kind, uint8_t sequence, UartErrCode status);
    void resendPendingIfNeeded();
    bool validateLength(UartMsgKind kind, uint8_t length) const;
    void emitError(UartErrCode code, uint8_t sequence, uint16_t detail, bool remote);
    void resetParser();
    uint8_t nextSequence();

    hal::ISerial* serial_   = nullptr;
    uint32_t      baudRate_ = 0;
    ParserCtx     parser_{};
    PendingFrame  pending_{};
    uint8_t       seqCounter_ = 0;

    HelloHandler           helloHandler_;
    HelloAckHandler        helloAckHandler_;
    TelemetryHandler       telemetryHandler_;
    CommandHandler         commandHandler_;
    ProfileHandler         profileHandler_;
    ConfigHandler          configHandler_;
    ConfigChunkHandler     configChunkHandler_;
    CommandAckHandler      commandAckHandler_;
    ConfigAckHandler       configAckHandler_;
    HeartbeatHandler       heartbeatHandler_;
    ErrorHandler           errorHandler_;
    LogHandler             logHandler_;
    WeightsHandler         weightsHandler_;
    StatusHandler          statusHandler_;
    RfidHandler            rfidHandler_;
    RfidDataHandler        rfidDataHandler_;
    ClaimStartHandler      claimStartHandler_;
    ClaimStatusHandler     claimStatusHandler_;
    ClaimCompleteHandler   claimCompleteHandler_;
    WsEnableHandler        wsEnableHandler_;
    WsStatusHandler        wsStatusHandler_;
    WsResetClientsHandler  wsResetClientsHandler_;
    WsStatusRequestHandler wsStatusRequestHandler_;
};

} // namespace idryer
