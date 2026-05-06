/**
 * @file link_integrations_store.h
 * @brief NVS persistence layer for LINK printer integrations (HA / Bambu / Moonraker).
 *
 * Stores each integration's config in its own NVS namespace so they are
 * independently readable and writable without loading the full config:
 *   - @c "li_ha"     — Home Assistant connection config
 *   - @c "li_bambu"  — Bambu Lab LAN config
 *   - @c "li_moon"   — Moonraker / Klipper config
 *   - @c "li_common" — active integration choice (@c CommonConfig)
 *
 * Normally you don't use this class directly — pass it to
 * @c LinkIntegrationsManager which calls load/save methods internally.
 * Declare it as a long-lived object (global or static member) alongside
 * @c LinkIntegrationsManager:
 *
 * @code
 * static idryer::cloud::LinkIntegrationsStore intStore;
 * static idryer::cloud::LinkIntegrationsManager intManager(&mqtt, &intStore);
 *
 * // in setup():
 * intManager.begin();  // calls intStore.begin() internally
 * @endcode
 */

#pragma once

#include "link_integrations_types.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

namespace idryer {
namespace cloud {

class LinkIntegrationsStore
{
public:
    LinkIntegrationsStore() = default;

    /// @brief Opens all NVS namespaces. Call once before any load/save. Returns @c false on NVS error.
    bool begin();

    // ── Home Assistant ────────────────────────────────────────────────────────

    bool loadHa(HaConfig& out) const;         ///< Reads HA config from NVS. Returns @c false if not stored.
    bool saveHa(const HaConfig& value);       ///< Writes HA config to NVS. Returns @c false on write error.
    void clearHa();                           ///< Erases the @c "li_ha" NVS namespace.

    // ── Bambu Lab ─────────────────────────────────────────────────────────────

    bool loadBambu(BambuConfig& out) const;   ///< Reads Bambu config from NVS. Returns @c false if not stored.
    bool saveBambu(const BambuConfig& value); ///< Writes Bambu config to NVS. Returns @c false on write error.
    void clearBambu();                        ///< Erases the @c "li_bambu" NVS namespace.

    // ── Moonraker / Klipper ───────────────────────────────────────────────────

    bool loadMoonraker(MoonrakerConfig& out) const;   ///< Reads Moonraker config. Returns @c false if not stored.
    bool saveMoonraker(const MoonrakerConfig& value); ///< Writes Moonraker config. Returns @c false on write error.
    void clearMoonraker();                            ///< Erases the @c "li_moon" NVS namespace.

    // ── Common ────────────────────────────────────────────────────────────────

    bool loadCommon(CommonConfig& out) const;   ///< Reads the active integration choice. Returns @c false if not stored.
    bool saveCommon(const CommonConfig& value); ///< Persists the active integration choice.
    void clearCommon();                         ///< Erases the @c "li_common" NVS namespace.

    /// @brief Erases all four NVS namespaces. Use to factory-reset integration configs.
    void clearAll();

private:
    static constexpr const char* kNamespaceHa        = "li_ha";
    static constexpr const char* kNamespaceBambu     = "li_bambu";
    static constexpr const char* kNamespaceMoonraker = "li_moon";
    static constexpr const char* kNamespaceCommon    = "li_common";
};

} // namespace cloud
} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
