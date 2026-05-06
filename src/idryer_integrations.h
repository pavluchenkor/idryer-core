/**
 * @file idryer_integrations.h
 * @brief Printer integration layer — Home Assistant, Bambu Lab, Moonraker (Klipper).
 *
 * Include alongside @c idryer_core.h on any device that needs to link with
 * a 3D printer or smart home system.
 *
 * Usage — wire into the product's command handler:
 * @code
 * // 1. Declare persistent storage and manager
 * idryer::cloud::LinkIntegrationsStore    intStore;
 * idryer::cloud::LinkIntegrationsManager intManager(&mqtt, &intStore);
 *
 * // 2. Dispatch integration commands inside the product handleCommand()
 * static void handleCommand(const char* cmd, JsonObjectConst data) {
 *     if (strcmp(cmd, "link_integration") == 0) {
 *         intManager.handleLinkIntegrationCommand(data); return;
 *     }
 *     if (strcmp(cmd, "bambu_apply") == 0) {
 *         intManager.handleBambuApplyCommand(data); return;
 *     }
 *     // ... other product commands ...
 * }
 *
 * // 3. Wire handleCommand to both transports and start
 * runtime.setCommandHandler(handleCommand);
 * local.setCommandSink(handleCommand);
 * intManager.begin();  // call after runtime.begin()
 * // in loop(): intManager.loop();
 * @endcode
 */

#pragma once

#include "integrations/common/link_integrations_types.h"
#include "integrations/common/link_integrations_store.h"
#include "integrations/common/link_integrations_manager.h"
