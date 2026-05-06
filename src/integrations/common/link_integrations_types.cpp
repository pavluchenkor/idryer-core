/**
 * @file link_integrations_types.cpp
 * @brief Реализация хелперов enum ↔ string.
 */

#include "link_integrations_types.h"
#include <string.h>

namespace idryer {
namespace cloud {

const char* activeIntegrationToString(ActiveIntegration value)
{
    switch (value)
    {
    case ActiveIntegration::None:      return "none";
    case ActiveIntegration::Ha:        return "ha";
    case ActiveIntegration::Bambu:     return "bambu";
    case ActiveIntegration::Moonraker: return "moonraker";
    }
    return "none";
}

bool activeIntegrationFromString(const char* str, ActiveIntegration& out)
{
    if (!str) return false;
    if (strcmp(str, "none") == 0)      { out = ActiveIntegration::None;      return true; }
    if (strcmp(str, "ha") == 0)        { out = ActiveIntegration::Ha;        return true; }
    if (strcmp(str, "bambu") == 0)     { out = ActiveIntegration::Bambu;     return true; }
    if (strcmp(str, "moonraker") == 0) { out = ActiveIntegration::Moonraker; return true; }
    return false;
}

const char* integrationStateToString(IntegrationState value)
{
    switch (value)
    {
    case IntegrationState::Disabled:      return "disabled";
    case IntegrationState::Idle:          return "idle";
    case IntegrationState::Connecting:    return "connecting";
    case IntegrationState::Online:        return "online";
    case IntegrationState::ConfigMissing: return "config_missing";
    case IntegrationState::Error:         return "error";
    }
    return "disabled";
}

} // namespace cloud
} // namespace idryer
