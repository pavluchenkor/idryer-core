#pragma once
#include "error_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ERRORBUS_SIZE
#define ERRORBUS_SIZE 16
#endif

typedef struct {
    uint32_t    ts_ms;
    ErrSeverity severity;
    ErrSource   source;
    ErrCode     code;
    const char* msg;
    int32_t     data;
    uint8_t     ctrl_id;  // unit/channel index (0-based)
} ErrorEvent;

void    errorbus_init(void);
void    errorbus_clear(void);
bool    errorbus_post(const ErrorEvent* e);
bool    errorbus_poll(ErrorEvent* out);
uint8_t errorbus_count(void);

#ifdef __cplusplus
}
#endif
