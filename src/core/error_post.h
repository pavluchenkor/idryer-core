#pragma once
#include "error_bus.h"

#if defined(ARDUINO)
#include <Arduino.h>
#define ERRBUS_NOW_MS() millis()
#else
#include <stdint.h>
extern uint32_t errbus_now_ms_impl(void);   // provide in host/test builds
#define ERRBUS_NOW_MS() errbus_now_ms_impl()
#endif

// ── Low-level post ────────────────────────────────────────────────────────────

static inline bool errbus_post_ex(uint8_t     ctrl_id,
                                  ErrSource   src,
                                  ErrSeverity sev,
                                  ErrCode     code,
                                  const char* msg,
                                  int32_t     data,
                                  uint32_t    now_ms)
{
    ErrorEvent e = { now_ms, sev, src, code, msg, data, ctrl_id };
    return errorbus_post(&e);
}

// ── Convenience wrappers (timestamp auto-filled from millis()) ────────────────

static inline bool POST_ERROR(ErrSeverity sev,
                              uint8_t     ctrl_id,
                              ErrSource   src,
                              ErrCode     code,
                              const char* msg,
                              int32_t     data)
{
    return errbus_post_ex(ctrl_id, src, sev, code, msg, data, ERRBUS_NOW_MS());
}

static inline bool post_info(uint8_t ctrl_id, ErrSource src, ErrCode code,
                             const char* msg, int32_t data)
{
    return errbus_post_ex(ctrl_id, src, ERRSEV_INFO, code, msg, data, ERRBUS_NOW_MS());
}

static inline bool post_warn(uint8_t ctrl_id, ErrSource src, ErrCode code,
                             const char* msg, int32_t data)
{
    return errbus_post_ex(ctrl_id, src, ERRSEV_WARNING, code, msg, data, ERRBUS_NOW_MS());
}

static inline bool post_error(uint8_t ctrl_id, ErrSource src, ErrCode code,
                              const char* msg, int32_t data)
{
    return errbus_post_ex(ctrl_id, src, ERRSEV_ERROR, code, msg, data, ERRBUS_NOW_MS());
}

static inline bool post_critical(uint8_t ctrl_id, ErrSource src, ErrCode code,
                                 const char* msg, int32_t data)
{
    return errbus_post_ex(ctrl_id, src, ERRSEV_CRITICAL, code, msg, data, ERRBUS_NOW_MS());
}
