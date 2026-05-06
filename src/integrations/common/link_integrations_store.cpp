/**
 * @file link_integrations_store.cpp
 * @brief Реализация NVS-хранилища LINK-интеграций.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "link_integrations_store.h"
#include "../../hal/hal_types.h"
#include <Preferences.h>
#include <string.h>

namespace idryer {
namespace cloud {

namespace {

void readString(Preferences& prefs, const char* key, char* buf, size_t bufSize, const char* dflt = "")
{
    String v = prefs.getString(key, dflt);
    size_t len = v.length();
    if (len >= bufSize) len = bufSize - 1;
    memcpy(buf, v.c_str(), len);
    buf[len] = '\0';
}

const char* mask(const char* value)
{
    return (value && value[0]) ? "****" : "(empty)";
}

} // namespace

bool LinkIntegrationsStore::begin()
{
    return true;
}

// =============================================================================
// Home Assistant
// =============================================================================

bool LinkIntegrationsStore::loadHa(HaConfig& out) const
{
    HaConfig fresh{};
    out = fresh;

    Preferences prefs;
    if (!prefs.begin(kNamespaceHa, /*readOnly=*/true)) {
        HAL_LOG_DEBUG("LINK_STORE", "HA: namespace empty");
        return false;
    }

    out.enabled = prefs.getBool("enabled", false);
    readString(prefs, "host", out.host, sizeof(out.host), "homeassistant.local");
    out.port = prefs.getUShort("port", 1883);
    readString(prefs, "username", out.username, sizeof(out.username));
    readString(prefs, "password", out.password, sizeof(out.password));
    readString(prefs, "discoveryPref", out.discoveryPrefix, sizeof(out.discoveryPrefix), "homeassistant");

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "HA loaded: enabled=%d host=%s port=%u user=%s pass=%s",
                 (int)out.enabled, out.host, out.port, out.username, mask(out.password));
    return out.configured();
}

bool LinkIntegrationsStore::saveHa(const HaConfig& value)
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceHa, /*readOnly=*/false)) {
        HAL_LOG_ERROR("LINK_STORE", "HA: failed to open namespace for write");
        return false;
    }

    prefs.putBool("enabled", value.enabled);
    prefs.putString("host", value.host);
    prefs.putUShort("port", value.port);
    prefs.putString("username", value.username);
    prefs.putString("password", value.password);
    prefs.putString("discoveryPref", value.discoveryPrefix);

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "HA saved: enabled=%d host=%s port=%u user=%s pass=%s",
                 (int)value.enabled, value.host, value.port, value.username, mask(value.password));
    return true;
}

void LinkIntegrationsStore::clearHa()
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceHa, false)) return;
    prefs.clear();
    prefs.end();
    HAL_LOG_INFO("LINK_STORE", "HA cleared");
}

// =============================================================================
// Bambu Lab
// =============================================================================

bool LinkIntegrationsStore::loadBambu(BambuConfig& out) const
{
    BambuConfig fresh{};
    out = fresh;

    Preferences prefs;
    if (!prefs.begin(kNamespaceBambu, true)) {
        HAL_LOG_DEBUG("LINK_STORE", "Bambu: namespace empty");
        return false;
    }

    out.enabled = prefs.getBool("enabled", false);
    readString(prefs, "ip", out.ip, sizeof(out.ip));
    readString(prefs, "serial", out.serial, sizeof(out.serial));
    readString(prefs, "lanAccessCode", out.lanAccessCode, sizeof(out.lanAccessCode));
    out.defaultAmsId = prefs.getUChar("defaultAmsId", 255);
    out.defaultTrayId = prefs.getUChar("defaultTrayId", 254);
    out.autoApplyOnTagDetect = prefs.getBool("autoApply", true);

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "Bambu loaded: enabled=%d ip=%s serial=%s code=%s ams=%u tray=%u auto=%d",
                 (int)out.enabled, out.ip, out.serial, mask(out.lanAccessCode),
                 out.defaultAmsId, out.defaultTrayId, (int)out.autoApplyOnTagDetect);
    return out.configured();
}

bool LinkIntegrationsStore::saveBambu(const BambuConfig& value)
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceBambu, false)) {
        HAL_LOG_ERROR("LINK_STORE", "Bambu: failed to open namespace for write");
        return false;
    }

    prefs.putBool("enabled", value.enabled);
    prefs.putString("ip", value.ip);
    prefs.putString("serial", value.serial);
    prefs.putString("lanAccessCode", value.lanAccessCode);
    prefs.putUChar("defaultAmsId", value.defaultAmsId);
    prefs.putUChar("defaultTrayId", value.defaultTrayId);
    prefs.putBool("autoApply", value.autoApplyOnTagDetect);

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "Bambu saved: enabled=%d ip=%s serial=%s code=%s",
                 (int)value.enabled, value.ip, value.serial, mask(value.lanAccessCode));
    return true;
}

void LinkIntegrationsStore::clearBambu()
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceBambu, false)) return;
    prefs.clear();
    prefs.end();
    HAL_LOG_INFO("LINK_STORE", "Bambu cleared");
}

// =============================================================================
// Moonraker
// =============================================================================

bool LinkIntegrationsStore::loadMoonraker(MoonrakerConfig& out) const
{
    MoonrakerConfig fresh{};
    out = fresh;

    Preferences prefs;
    if (!prefs.begin(kNamespaceMoonraker, true)) {
        HAL_LOG_DEBUG("LINK_STORE", "Moonraker: namespace empty");
        return false;
    }

    out.enabled = prefs.getBool("enabled", false);
    readString(prefs, "host", out.host, sizeof(out.host));
    out.port = prefs.getUShort("port", 7125);
    readString(prefs, "apiKey", out.apiKey, sizeof(out.apiKey));
    out.ssl = prefs.getBool("ssl", false);
    out.pollIntervalMs = prefs.getULong("pollInterval", 1000);

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "Moonraker loaded: enabled=%d host=%s port=%u ssl=%d apiKey=%s poll=%u",
                 (int)out.enabled, out.host, out.port, (int)out.ssl,
                 mask(out.apiKey), out.pollIntervalMs);
    return out.configured();
}

bool LinkIntegrationsStore::saveMoonraker(const MoonrakerConfig& value)
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceMoonraker, false)) {
        HAL_LOG_ERROR("LINK_STORE", "Moonraker: failed to open namespace for write");
        return false;
    }

    prefs.putBool("enabled", value.enabled);
    prefs.putString("host", value.host);
    prefs.putUShort("port", value.port);
    prefs.putString("apiKey", value.apiKey);
    prefs.putBool("ssl", value.ssl);
    prefs.putULong("pollInterval", value.pollIntervalMs);

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "Moonraker saved: enabled=%d host=%s port=%u ssl=%d",
                 (int)value.enabled, value.host, value.port, (int)value.ssl);
    return true;
}

void LinkIntegrationsStore::clearMoonraker()
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceMoonraker, false)) return;
    prefs.clear();
    prefs.end();
    HAL_LOG_INFO("LINK_STORE", "Moonraker cleared");
}

// =============================================================================
// Common
// =============================================================================

bool LinkIntegrationsStore::loadCommon(CommonConfig& out) const
{
    CommonConfig fresh{};
    out = fresh;

    Preferences prefs;
    if (!prefs.begin(kNamespaceCommon, true)) {
        HAL_LOG_DEBUG("LINK_STORE", "Common: namespace empty");
        return false;
    }

    uint8_t activeRaw = prefs.getUChar("active", static_cast<uint8_t>(ActiveIntegration::None));
    if (activeRaw <= static_cast<uint8_t>(ActiveIntegration::Moonraker)) {
        out.active = static_cast<ActiveIntegration>(activeRaw);
    }

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "Common loaded: active=%s", activeIntegrationToString(out.active));
    return true;
}

bool LinkIntegrationsStore::saveCommon(const CommonConfig& value)
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceCommon, false)) {
        HAL_LOG_ERROR("LINK_STORE", "Common: failed to open namespace for write");
        return false;
    }

    prefs.putUChar("active", static_cast<uint8_t>(value.active));

    prefs.end();

    HAL_LOG_INFO("LINK_STORE", "Common saved: active=%s", activeIntegrationToString(value.active));
    return true;
}

void LinkIntegrationsStore::clearCommon()
{
    Preferences prefs;
    if (!prefs.begin(kNamespaceCommon, false)) return;
    prefs.clear();
    prefs.end();
    HAL_LOG_INFO("LINK_STORE", "Common cleared");
}

// =============================================================================
// Всё сразу
// =============================================================================

void LinkIntegrationsStore::clearAll()
{
    clearHa();
    clearBambu();
    clearMoonraker();
    clearCommon();
    HAL_LOG_INFO("LINK_STORE", "All sections cleared");
}

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
