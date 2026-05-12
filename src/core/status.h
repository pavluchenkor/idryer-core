#pragma once
#include <stdbool.h>
#include "error_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { bool ok; ErrCode code; const char* msg; } Status;
typedef struct { bool ok; float value; ErrCode code; const char* msg; } ResultF;

#ifdef __cplusplus
}

inline Status  StatusOK()                            { return {true,  ERRC_OK,  ""}; }
inline Status  StatusErr(ErrCode c, const char* m)   { return {false, c,         m}; }
inline ResultF ResultFOk(float v)                    { return {true,  v, ERRC_OK, ""}; }
inline ResultF ResultFErr(ErrCode c, const char* m)  { return {false, 0.0f, c,    m}; }
#endif
