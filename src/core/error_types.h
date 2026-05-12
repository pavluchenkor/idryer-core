#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "error_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define X(en, s) en,
  ERRSEV_LIST(X)
#undef X
} ErrSeverity;

typedef enum {
#define X(en, s) en,
  ERRSRC_LIST(X)
#undef X
} ErrSource;

typedef enum {
#define X(en, name, human) en,
  ERRCODE_LIST(X)
#undef X
} ErrCode;

#ifdef __cplusplus
}
#endif
