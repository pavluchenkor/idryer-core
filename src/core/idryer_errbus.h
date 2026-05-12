#pragma once

/**
 * @file idryer_errbus.h
 * @brief Single include for the idryer error bus subsystem.
 *
 * Usage (in any device firmware — iHeater, Storage, Dryer, etc.):
 *
 *   // 1. In setup(): register the output handler
 *   error_set_handler([](const ErrorEvent* ev) {
 *       // → forward to MQTT / UART / local display
 *       link.raiseEvent(...);
 *
 *       // → react to high severity inside the device
 *       if (ev->severity == ERRSEV_CRITICAL) {
 *           heater.stop();
 *           ESP.restart();
 *       }
 *   });
 *   error_set_min_severity(ERRSEV_WARNING);  // optional: suppress INFO
 *
 *   // 2. In loop(): drain the bus
 *   error_process_all();
 *
 *   // 3. From any sensor/module: post an event
 *   post_error(unitId, ERRSRC_THERM, ERRC_OVER_MAX, "thermistor overmax", tempC10);
 *   post_critical(0, ERRSRC_HEATER, ERRC_NO_RESPONSE, "heater lost", 0);
 */

#include "error_defs.h"
#include "error_types.h"
#include "error_bus.h"
#include "error_post.h"
#include "error_table.h"
#include "error_process.h"
#include "status.h"
