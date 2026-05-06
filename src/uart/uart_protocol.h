#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @file uart_protocol.h
 * @brief Shared UART frame contract between RP2040 (Controller) and ESP32 (Link).
 *
 * This header is usable on both MCUs — it has no platform-specific dependencies.
 *
 * Frame layout (over 115200 baud, 8N1, no hardware flow control):
 * @code
 * byte 0  : start of frame (0xAA)
 * byte 1  : protocol version (UART_PROTOCOL_VER)
 * byte 2  : flags  (bit0 = ACK required, bit1 = is ACK, bit2 = error,
 *                   bit3 = fragment, bit4 = last fragment)
 * byte 3  : message type (UartMsgKind)
 * byte 4  : sequence number (0..255, wraps)
 * byte 5  : payload length (0..UART_MAX_PAYLOAD)
 * payload : message-specific data (see payload structs below)
 * crc16   : CRC-16/CCITT, low byte then high byte (poly 0x1021, init 0xFFFF)
 * @endcode
 *
 * Adding a new message type:
 *   1. Add a value to @c UartMsgKind.
 *   2. Define a payload struct with @c __attribute__((packed)) if needed.
 *   3. Add a @c static_assert for the struct size.
 *   4. Add a handler type and @c set*Handler() in @c UartBridge.
 */

namespace idryer {

constexpr uint8_t UART_SOF             = 0xAA;   ///< Start-of-frame byte.
constexpr uint8_t UART_PROTOCOL_VER   = 1;       ///< Current protocol version.

/// @name Флаги UART-фрейма
/// @{
constexpr uint8_t UART_FLAG_ACK_REQ       = 0x01;  ///< Отправитель ждёт ACK на этот фрейм.
constexpr uint8_t UART_FLAG_IS_ACK        = 0x02;  ///< Этот фрейм сам является ACK.
constexpr uint8_t UART_FLAG_ERROR         = 0x04;  ///< Полезная нагрузка содержит ошибку.
constexpr uint8_t UART_FLAG_FRAGMENT      = 0x08;  ///< Фрейм — часть фрагментированной передачи.
/**
 * @brief Маркер последнего фрагмента в фрагментированной передаче.
 *
 * Выставляется на финальном фрейме передачи, разбитой через @c UART_FLAG_FRAGMENT.
 * Принимающая сторона по этому флагу понимает, что можно собирать сообщение
 * целиком и отправлять ACK всей передачи.
 */
constexpr uint8_t UART_FLAG_LAST_FRAGMENT = 0x10;
/// @}

constexpr uint8_t  UART_MAX_PAYLOAD        = 200;   ///< Max payload bytes per frame.
constexpr uint8_t  UART_MAX_RETRIES        = 3;     ///< ACK retry limit.
constexpr uint16_t UART_CMD_TIMEOUT_MS     = 700;   ///< ACK wait timeout (ms).
constexpr uint16_t UART_HEARTBEAT_MS       = 5000;  ///< Heartbeat send interval (ms).
constexpr uint32_t UART_LINK_LOSS_MS       = 20000; ///< No heartbeat = link lost (ms).
constexpr uint16_t UART_HELLO_INTERVAL_MS  = 5000;  ///< Hello retry interval (ms).
constexpr uint8_t  UART_HELLO_MAX_ATTEMPTS = 12;    ///< Max Hello attempts before giving up.

/// @brief Identifies which side sent a Hello frame.
enum class UartRole : uint8_t {
    Rp2040Controller = 0x01,
    EspBridge        = 0x02,
    HelloRequest     = 0xFF,
};

/// @brief Device types — used in Hello to identify the hardware.
enum class UartDeviceType : uint8_t {
    Unknown    = 0x00,
    Dryer      = 0x01,
    Heater     = 0x02,
    Telemetry  = 0x03,
    Link       = 0x04,
    IHeaterLink = 0x05,
};

/// @brief Message type identifiers.
enum class UartMsgKind : uint8_t {
    Hello         = 0x01,  ///< RP2040 announces itself; ESP32 responds with HelloAck.
    HelloAck      = 0x02,  ///< ESP32 responds with IP and SSID.
    Telemetry     = 0x10,  ///< Sensor readings (temp, humidity, heater power).
    TelemetryAck  = 0x11,
    Weights       = 0x12,  ///< Scale readings.
    Status        = 0x13,  ///< Dryer session state.
    Rfid          = 0x14,  ///< RFID tag event.
    RfidReadData  = 0x1A,  ///< RFID tag data block.
    RfidWriteData = 0x1B,
    Command       = 0x20,  ///< Backend command forwarded to RP2040.
    CommandAck    = 0x21,
    ConfigPush    = 0x30,  ///< Config pushed to RP2040.
    ConfigAck     = 0x31,
    Heartbeat     = 0x40,  ///< Keep-alive + uptime.
    Error         = 0x50,
    Log           = 0x60,  ///< Debug log string from RP2040.
    ClaimStart    = 0x70,  ///< RP2040 requests device claim.
    ClaimStatus   = 0x71,
    ClaimComplete = 0x72,
    WsEnable         = 0x73,  ///< Enable/disable WebSocket server.
    WsStatus         = 0x74,
    WsResetClients   = 0x75,
    WsStatusRequest  = 0x76,
};

/// @brief Dryer operational modes reported in @c UartStatusEntry.
enum class UartDryerMode : uint8_t {
    Idle    = 0,
    Drying  = 1,
    Storage = 2,
    Profile = 3,
    Fault   = 4,
};

enum class UartStagePhase : uint8_t {
    Ramp = 0,
    Hold = 1,
};

/// @brief Commands forwarded from the backend to the RP2040.
enum class UartCmdCode : uint8_t {
    Start       = 0x01,
    Stop        = 0x02,
    Find        = 0x03,
    GetConfig   = 0x05,
    SetConfig   = 0x06,
    ReadRfid    = 0x07,
    WriteRfid   = 0x08,
    ResetFault  = 0x10,
    WifiStatus  = 0x11,
    ClearErrors = 0x12,
};

/// @brief Error codes used in ACK and error frames.
enum class UartErrCode : uint8_t {
    None             = 0x00,
    CrcMismatch      = 0x01,
    UnknownMessage   = 0x02,
    InvalidPayload   = 0x03,
    Busy             = 0x04,
    Timeout          = 0x05,
    SequenceMismatch = 0x06,
};

/// @brief Claim lifecycle states reported back to the RP2040.
enum class UartClaimStatus : uint8_t {
    Idle         = 0x00,
    Provisioning = 0x01,
    WaitingClaim = 0x02,
    Claimed      = 0x03,
    Error        = 0x04,
};

/// @brief Cloud connectivity state sent in Heartbeat frames to the RP2040.
enum class UartLinkCloudState : uint8_t {
    Idle           = 0,
    WifiConnecting = 1,
    Provisioning   = 2,
    Registering    = 3,
    AwaitingClaim  = 4,
    Ready          = 5,
    MqttConnecting = 6,
    Online         = 7,
};

/// @brief Capability flags reported in Hello — bitmask of available hardware.
namespace UartUnitCaps {
    constexpr uint16_t HEATER           = (1 << 0);
    constexpr uint16_t FAN              = (1 << 1);
    constexpr uint16_t SERVO            = (1 << 2);
    constexpr uint16_t RH_AIR_SENSOR    = (1 << 3);
    constexpr uint16_t TEMP_AIR_SENSOR  = (1 << 4);
    constexpr uint16_t TEMP_HTR_SENSOR  = (1 << 5);
}

#pragma pack(push, 1)

struct UartUnitConfig {
    uint8_t  unitId;
    uint8_t  _pad1;
    uint16_t capabilities;
    uint8_t  scales[4];
    uint8_t  rfid[4];
} __attribute__((packed));

struct UartFrameHeader {
    uint8_t     sof;
    uint8_t     version;
    uint8_t     flags;
    UartMsgKind kind;
    uint8_t     sequence;
    uint8_t     payloadLength;
};

struct UartFrame {
    UartFrameHeader header;
    uint8_t         payload[UART_MAX_PAYLOAD];
    uint16_t        crc;
};

/// @brief Sent by RP2040 on boot. Describes the hardware to the ESP32.
struct UartHelloPayload {
    UartRole   role;
    uint8_t    deviceType;
    uint8_t    _pad1[2];
    uint32_t   firmwareVersion;
    uint32_t   workTimeCounter;
    char       hardwareVersion[8];
    uint8_t    unitsCount;
    UartUnitConfig units[4];
    char       mcuSerial[17];
} __attribute__((packed));

/// @brief ESP32 response to Hello. Carries WiFi status for display.
struct UartHelloAckPayload {
    uint32_t ipAddress;
    char     ssid[33];
};

struct UartTelemetryEntry {
    uint8_t  unitId;
    int16_t  temperatureC10;   ///< Temperature × 10 (°C).
    uint16_t humidityPct10;    ///< Relative humidity × 10 (%).
    uint8_t  heaterPowerPct;
    uint8_t  fanOn;
};

struct UartTelemetryPayload {
    uint8_t            count;
    UartTelemetryEntry units[4];
};

struct UartCmdPayload {
    UartCmdCode command;
    uint8_t     targetState;
    uint8_t     unitId;
    uint8_t     reserved[2];
    uint32_t    arg0;
    uint32_t    arg1;
};

struct UartConfigPayload {
    int16_t  targetTemperatureC10;  ///< Target temperature × 10 (°C).
    uint16_t targetHumidityPct;
    uint16_t durationMinutes;
    uint16_t fanDutyPct;
};

struct UartHeartbeatPayload {
    uint32_t uptimeSeconds;
    int16_t  wifiRssiDbm;
    uint16_t errorsSinceBoot;
    uint8_t  cloudState;
};

struct UartAckPayload {
    uint8_t     ackSequence;
    UartErrCode status;
};

struct UartErrorPayload {
    UartErrCode code;
    uint8_t     lastSequence;
    uint16_t    detail;
};

struct UartStatusEntry {
    uint8_t        unitId;
    UartDryerMode  mode;
    uint32_t       sessionNum;
    int16_t        targetTempC10;
    uint16_t       targetHumidityPct;
    uint16_t       durationMinutes;
    uint32_t       elapsedSeconds;
    uint32_t       stageElapsedSeconds;
    uint32_t       stageRemainingSeconds;
    uint32_t       totalRemainingSeconds;
    uint8_t        currentStage;
    uint8_t        totalStages;
    UartStagePhase stagePhase;
    uint8_t        _pad;
};

struct UartStatusPayload {
    uint8_t         count;
    UartStatusEntry units[4];
    uint32_t        uptime;
};

struct UartWeightEntry {
    uint8_t  sensorId;
    uint8_t  unitId;
    uint16_t weightGramsC10;  ///< Weight × 10 (grams).
};

struct UartWeightsPayload {
    uint8_t          count;
    UartWeightEntry  weights[4];
};

struct UartRfidPayload {
    uint8_t   event;
    uint8_t   readerId;
    char      tag[32];
    uint8_t   unitId;
    uint8_t   _pad[2];
};

struct UartRfidDataPayload {
    uint8_t readerId;
    uint8_t unitId;
    char    tag[32];
    uint8_t fragment[163];
    uint8_t _pad[2];
};

struct UartProfileStage {
    uint16_t temp;  ///< Target temperature (°C).
    uint16_t ramp;  ///< Ramp duration (minutes).
    uint16_t hold;  ///< Hold duration (minutes).
};

struct UartProfilePayload {
    uint8_t          unitId;
    uint8_t          totalStages;
    uint8_t          startStage;
    uint8_t          _pad;
    UartProfileStage stages[10];
};

struct UartClaimStatusPayload {
    UartClaimStatus status;
    char            pin[9];
    uint32_t        expiresAt;
    uint32_t        remainingSeconds;
};

struct UartClaimCompletePayload {
    uint8_t success;
    char    deviceId[37];
};

struct UartWsEnablePayload {
    uint8_t  enable;
    uint8_t  reserved;
    uint16_t pin;
};

enum class UartWsState : uint8_t {
    Disabled  = 0,
    Listening = 1,
    Connected = 2,
};

struct UartWsStatusPayload {
    UartWsState state;
    uint16_t    pin;
    uint8_t     pairedCount;
    uint8_t     maxClients;
    uint8_t     reserved;
};

/// @brief Размер заголовка конфиг-чанка @c UartConfigChunkHeader в байтах.
///
/// Каждый фрейм передачи конфига несёт заголовок (transferId, totalSize, chunkIndex)
/// и потом данные. Эта константа — длина заголовочной части, она же — смещение,
/// откуда начинаются полезные данные внутри @c UartConfigChunkPayload.
constexpr uint8_t UART_CONFIG_CHUNK_HEADER_SIZE = 6;

/// @brief Максимум полезных данных в одном конфиг-чанке (после заголовка).
constexpr uint8_t UART_CONFIG_CHUNK_DATA_SIZE   = UART_MAX_PAYLOAD - UART_CONFIG_CHUNK_HEADER_SIZE;

struct UartConfigChunkHeader {
    uint16_t transferId;
    uint16_t totalSize;
    uint16_t chunkIndex;
};

struct UartConfigChunkPayload {
    UartConfigChunkHeader header;
    uint8_t               data[UART_CONFIG_CHUNK_DATA_SIZE];
};

#pragma pack(pop)

// Compile-time size checks
static_assert(sizeof(UartUnitConfig)         == 12,  "UartUnitConfig must be 12 bytes");
static_assert(sizeof(UartHelloPayload)       == 86,  "UartHelloPayload must be 86 bytes");
static_assert(sizeof(UartFrameHeader)        == 6,   "UartFrameHeader must be 6 bytes");
static_assert(sizeof(UartConfigChunkHeader)  == UART_CONFIG_CHUNK_HEADER_SIZE, "UartConfigChunkHeader must be 6 bytes");
static_assert(sizeof(UartHelloPayload)       <= UART_MAX_PAYLOAD, "UartHelloPayload exceeds UART_MAX_PAYLOAD");
static_assert(sizeof(UartTelemetryPayload)   <= UART_MAX_PAYLOAD, "UartTelemetryPayload exceeds UART_MAX_PAYLOAD");
static_assert(sizeof(UartStatusPayload)      <= UART_MAX_PAYLOAD, "UartStatusPayload exceeds UART_MAX_PAYLOAD");
static_assert(sizeof(UartProfilePayload)     <= UART_MAX_PAYLOAD, "UartProfilePayload exceeds UART_MAX_PAYLOAD");
static_assert(sizeof(UartRfidDataPayload)    <= UART_MAX_PAYLOAD, "UartRfidDataPayload exceeds UART_MAX_PAYLOAD");
static_assert(sizeof(UartConfigChunkPayload) <= UART_MAX_PAYLOAD, "UartConfigChunkPayload exceeds UART_MAX_PAYLOAD");

/// @brief Computes CRC-16/CCITT (poly 0x1021, init 0xFFFF) over @p data.
uint16_t uartCalculateCrc(const uint8_t* data, size_t length);

/// @brief Returns @c true if the frame flags indicate the sender expects an ACK.
inline bool uartRequiresAck(uint8_t flags) { return (flags & UART_FLAG_ACK_REQ) != 0; }

} // namespace idryer
