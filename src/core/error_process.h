#pragma once
#include "error_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ErrorCallback)(const ErrorEvent* e);

// Register the output handler. Called for every event that passes filters.
void error_set_handler(ErrorCallback cb);

// Only forward events with severity >= min_sev (default: ERRSEV_INFO = all).
void error_set_min_severity(ErrSeverity min_sev);

// ctrl_id filter: enable/disable forwarding for a specific unit (0..7).
void error_enable_ctrl(uint8_t id);
void error_disable_ctrl(uint8_t id);

// Call this in the main loop. Drains the bus and invokes the registered handler.
void error_process_all(void);

#ifdef __cplusplus
}
#endif
