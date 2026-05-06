/**
 * @file link_integrations_types.h
 * @brief Config structs and enums for LINK printer integrations (HA / Bambu / Moonraker).
 *
 * These types are part of the public integrations API. You'll use them when
 * reading or writing integration configs through @c LinkIntegrationsStore,
 * and when subscribing to status callbacks from @c LinkIntegrationsManager.
 */

#pragma once

#include <stdint.h>

namespace idryer {
namespace cloud {

/**
 * @brief Which printer integration is currently active.
 *
 * Only one integration can be active at a time. Controlled via the
 * @c commands/link_integration MQTT command with @c {"active": "ha"|"bambu"|"moonraker"|"none"}.
 */
enum class ActiveIntegration : uint8_t
{
    None      = 0,
    Ha        = 1,
    Bambu     = 2,
    Moonraker = 3,
};

const char* activeIntegrationToString(ActiveIntegration value);
bool activeIntegrationFromString(const char* str, ActiveIntegration& out);

// ── Home Assistant ─────────────────────────────────────────────────────────────

/// @brief Параметры подключения к MQTT-брокеру Home Assistant.
struct HaConfig
{
    bool     enabled             = false;       ///< Включить интеграцию HA, когда она выбрана активной.
    char     host[64]            = {0};         ///< Адрес HA MQTT-брокера (hostname или IP).
    uint16_t port                = 1883;        ///< Порт HA MQTT-брокера. По умолчанию 1883 (8883 для TLS).
    char     username[33]        = {0};         ///< Имя пользователя HA MQTT (опционально).
    char     password[65]        = {0};         ///< Пароль HA MQTT.
    char     discoveryPrefix[24] = {0};         ///< Префикс MQTT Discovery. По умолчанию @c "homeassistant".

    /// @brief Возвращает @c true, если задан хост.
    bool configured() const { return host[0] != '\0'; }
};

// ── Bambu Lab ─────────────────────────────────────────────────────────────────

/// @brief Параметры подключения к принтеру Bambu Lab по LAN.
struct BambuConfig
{
    bool     enabled              = false;      ///< Включить интеграцию Bambu, когда она выбрана активной.
    char     ip[16]               = {0};        ///< IPv4-адрес принтера (например, "192.168.1.42").
    char     serial[20]           = {0};        ///< Серийный номер принтера Bambu (~15 символов).
    char     lanAccessCode[16]    = {0};        ///< 8-значный LAN access code с экрана принтера.
    uint8_t  defaultAmsId         = 255;        ///< Дефолтный слот AMS при apply (0xFF = «использовать дефолт»).
    uint8_t  defaultTrayId        = 254;        ///< Дефолтный tray ID внутри AMS (0xFE = «использовать дефолт»).
    bool     autoApplyOnTagDetect = true;       ///< Автоматически вызывать applyFilament при чтении RFID-метки.

    /// @brief Возвращает @c true, если заданы все обязательные поля.
    bool configured() const { return ip[0] != '\0' && serial[0] != '\0' && lanAccessCode[0] != '\0'; }
};

// ── Moonraker / Klipper ───────────────────────────────────────────────────────

/// @brief Параметры подключения к Moonraker (Klipper).
struct MoonrakerConfig
{
    bool     enabled        = false;            ///< Включить интеграцию Moonraker, когда она выбрана активной.
    char     host[64]       = {0};              ///< Адрес Moonraker (hostname или IP).
    uint16_t port           = 7125;             ///< Порт HTTP/WebSocket Moonraker. По умолчанию 7125.
    char     apiKey[65]     = {0};              ///< Moonraker API key (опционально).
    bool     ssl            = false;            ///< Использовать @c wss:// вместо @c ws://.
    uint32_t pollIntervalMs = 1000;             ///< Период опроса статуса в мс. По умолчанию 1 с.

    /// @brief Возвращает @c true, если задан хост.
    bool configured() const { return host[0] != '\0'; }
};

// ── Common ─────────────────────────────────────────────────────────────────────

/// @brief Holds the active integration choice. Persisted in NVS.
struct CommonConfig
{
    ActiveIntegration active = ActiveIntegration::None;
};

// ── Bambu apply payload ───────────────────────────────────────────────────────

/**
 * @brief Payload for the @c commands/bambu_apply MQTT command.
 *
 * Tells the Bambu client which AMS slot to load and what filament metadata to write.
 * If @c amsId or @c trayId is @c null in the JSON, the manager substitutes the
 * default values from @c BambuConfig.
 */
constexpr uint8_t kBambuApplyAmsFromConfig  = 0xFF;
constexpr uint8_t kBambuApplyTrayFromConfig = 0xFE;

struct BambuApplyPayload
{
    uint8_t  amsId         = kBambuApplyAmsFromConfig;
    uint8_t  trayId        = kBambuApplyTrayFromConfig;
    char     trayType[16]  = {0};
    char     colorHex[10]  = {0};
    uint16_t nozzleTempMin = 0;
    uint16_t nozzleTempMax = 0;
    char     trayInfoIdx[8]= {0};
    char     settingId[32] = {0};
    char     spoolId[40]   = {0};
    char     uid[32]       = {0};

    bool valid() const
    {
        return trayType[0] != '\0' && colorHex[0] != '\0'
            && nozzleTempMin > 0 && nozzleTempMax > 0
            && trayInfoIdx[0] != '\0';
    }
};

// ── Runtime state ─────────────────────────────────────────────────────────────

/**
 * @brief Connection state for each integration section.
 *
 * Published as part of @c idryer/{serial}/integrations/status.
 */
enum class IntegrationState : uint8_t
{
    Disabled,      ///< This integration is not the active one.
    Idle,          ///< Active and configured, but not yet connected.
    Connecting,    ///< Connection attempt in progress.
    Online,        ///< Connected and operational.
    ConfigMissing, ///< Active but required fields are empty or invalid.
    Error,         ///< Connection failed; see @c lastError in status publish.
};

const char* integrationStateToString(IntegrationState value);

} // namespace cloud
} // namespace idryer
