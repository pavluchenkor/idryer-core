#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include "../uart/uart_protocol.h"

namespace idryer {

constexpr uint16_t CONFIG_BUFFER_SIZE = 16384;

/**
 * @brief Result codes returned by @c ConfigReceiver::processFragment().
 */
enum class ConfigFragResult : uint8_t {
    Ok              = 0,  ///< Fragment accepted, transfer in progress.
    Complete        = 1,  ///< Last fragment received; full JSON is ready in @c getJson().
    ErrorSequence   = 2,  ///< Fragment arrived out of order.
    ErrorOverflow   = 3,  ///< Total size exceeds @c CONFIG_BUFFER_SIZE.
    ErrorTransferId = 4,  ///< Fragment belongs to an unknown transfer ID.
    ErrorSizeMismatch = 5, ///< Final size doesn't match declared total size.
};

/**
 * @brief Reassembles a fragmented config JSON from incoming @c UartConfigChunkPayload frames.
 *
 * The RP2040 sends large JSON configs in chunks over UART. Feed each chunk into
 * @c processFragment() and wait for @c ConfigFragResult::Complete, then read
 * the assembled JSON from @c getJson().
 *
 * Example:
 * @code
 * ConfigReceiver rx;
 * bridge.setConfigChunkHandler([&rx](const UartConfigChunkPayload& p, uint8_t len, const UartFrameHeader& hdr) {
 *     auto result = rx.processFragment(p, len, hdr.flags);
 *     if (result == ConfigFragResult::Complete) {
 *         mqtt.publishConfigRaw(rx.getJson(), rx.getLength());
 *         rx.reset();
 *     }
 * });
 * @endcode
 */
class ConfigReceiver {
public:
    /// @brief Resets the receiver. Safe to call at any time.
    void reset() {
        transferId_   = 0;
        totalSize_    = 0;
        receivedSize_ = 0;
        nextChunk_    = 0;
        active_       = false;
    }

    /**
     * @brief Processes one incoming config chunk.
     * @param payload  The chunk payload from the UART frame.
     * @param dataLen  Number of data bytes in @c payload.data.
     * @param flags    Frame flags from @c UartFrameHeader::flags (used to detect last fragment).
     * @return @c ConfigFragResult::Complete when all fragments have been received.
     */
    ConfigFragResult processFragment(const UartConfigChunkPayload& payload,
                                      uint8_t dataLen, uint8_t flags) {
        const auto& hdr = payload.header;

        if (hdr.chunkIndex == 0) {
            transferId_   = hdr.transferId;
            totalSize_    = hdr.totalSize;
            receivedSize_ = 0;
            nextChunk_    = 0;
            active_       = true;
            if (totalSize_ > CONFIG_BUFFER_SIZE) { reset(); return ConfigFragResult::ErrorOverflow; }
        } else {
            if (!active_ || hdr.transferId != transferId_)
                return ConfigFragResult::ErrorTransferId;
        }

        if (hdr.chunkIndex != nextChunk_) return ConfigFragResult::ErrorSequence;

        if (receivedSize_ + dataLen > CONFIG_BUFFER_SIZE) {
            reset(); return ConfigFragResult::ErrorOverflow;
        }

        memcpy(buffer_ + receivedSize_, payload.data, dataLen);
        receivedSize_ += dataLen;
        nextChunk_++;

        if (flags & UART_FLAG_LAST_FRAGMENT) {
            if (totalSize_ > 0 && receivedSize_ != totalSize_) {
                reset();
                return ConfigFragResult::ErrorSizeMismatch;
            }
            if (receivedSize_ < CONFIG_BUFFER_SIZE) buffer_[receivedSize_] = '\0';
            return ConfigFragResult::Complete;
        }
        return ConfigFragResult::Ok;
    }

    /// @brief Returns a pointer to the assembled JSON. Valid only after @c Complete.
    const char* getJson()    const { return reinterpret_cast<const char*>(buffer_); }

    /// @brief Returns the length of the assembled JSON in bytes.
    uint16_t    getLength()  const { return receivedSize_; }

    /// @brief Returns @c true if a transfer is currently in progress.
    bool        isActive()   const { return active_; }

    uint16_t    transferId() const { return transferId_; }

private:
    uint8_t  buffer_[CONFIG_BUFFER_SIZE]{};
    uint16_t transferId_   = 0;
    uint16_t totalSize_    = 0;
    uint16_t receivedSize_ = 0;
    uint16_t nextChunk_    = 0;
    bool     active_       = false;
};

/**
 * @brief Splits a JSON string into @c UartConfigChunkPayload frames and sends them.
 *
 * Use alongside @c UartBridge::sendConfigPushChunk() to push a full config JSON
 * from the ESP32 to the RP2040 (or in reverse, in tests).
 *
 * Example:
 * @code
 * ConfigSender tx;
 * uint16_t tid = ConfigSender::generateTransferId();
 * tx.send(json, length, tid, [&](const UartConfigChunkPayload& p, uint8_t len, uint8_t flags) {
 *     return bridge.sendConfigPushChunk(p, len, flags);
 * });
 * @endcode
 */
class ConfigSender {
public:
    /**
     * @brief Function called for each outgoing chunk.
     * @param payload    The chunk to send.
     * @param payloadLen Total payload length (header + data).
     * @param flags      Frame flags (@c UART_FLAG_FRAGMENT, optionally @c UART_FLAG_LAST_FRAGMENT).
     * @return @c true to continue, @c false to abort the transfer.
     */
    using SendFn = std::function<bool(const UartConfigChunkPayload&, uint8_t payloadLen, uint8_t flags)>;

    /**
     * @brief Sends @p json as a series of config chunks via @p sendFn.
     * @param json       JSON string to send.
     * @param length     Length of @p json in bytes.
     * @param transferId Transfer identifier (use @c generateTransferId()).
     * @param sendFn     Function that transmits each chunk.
     * @return Number of chunks sent, or @c 0 on failure.
     */
    uint16_t send(const char* json, uint16_t length, uint16_t transferId, SendFn sendFn) {
        if (!json || length == 0 || !sendFn) return 0;

        uint16_t offset     = 0;
        uint16_t chunkIndex = 0;
        uint16_t sentCount  = 0;

        while (offset < length) {
            UartConfigChunkPayload payload{};
            payload.header.transferId  = transferId;
            payload.header.totalSize   = (chunkIndex == 0) ? length : 0;
            payload.header.chunkIndex  = chunkIndex;

            uint16_t remaining = length - offset;
            uint8_t  dataLen   = (remaining > UART_CONFIG_CHUNK_DATA_SIZE)
                                 ? UART_CONFIG_CHUNK_DATA_SIZE
                                 : static_cast<uint8_t>(remaining);

            memcpy(payload.data, json + offset, dataLen);

            uint8_t flags = UART_FLAG_FRAGMENT;
            if (offset + dataLen >= length) flags |= UART_FLAG_LAST_FRAGMENT;

            uint8_t payloadLen = UART_CONFIG_CHUNK_HEADER_SIZE + dataLen;
            if (!sendFn(payload, payloadLen, flags)) return 0;

            offset += dataLen;
            chunkIndex++;
            sentCount++;
        }
        return sentCount;
    }

    /// @brief Returns a monotonically increasing transfer ID. Thread-unsafe (single-core OK).
    static uint16_t generateTransferId() {
        static uint16_t counter = 0;
        return ++counter;
    }
};

} // namespace idryer
