#pragma once
#include <stddef.h>
#include "error_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

const char* errsev_name(ErrSeverity s);   // "INFO" / "WARN" / "ERROR" / "CRIT"
const char* errsrc_name(ErrSource s);     // "HEATER" / "SHT" / ...
const char* errcode_name(ErrCode c);      // "OVER_MAX" / ...
const char* errcode_human(ErrCode c);     // "Value over maximum" / ...

// Format full event line into buf. Returns number of chars written (excl. \0).
int error_format_line(const ErrorEvent* ev, char* buf, size_t buf_sz);

#ifdef __cplusplus
}
#endif
