#include "error_table.h"
#include "error_defs.h"
#include <stdio.h>

const char* errsev_name(ErrSeverity s) {
    switch (s) {
#define X(en, str) case en: return str;
        ERRSEV_LIST(X)
#undef X
        default: return "SEV?";
    }
}

const char* errsrc_name(ErrSource s) {
    switch (s) {
#define X(en, str) case en: return str;
        ERRSRC_LIST(X)
#undef X
        default: return "SRC?";
    }
}

const char* errcode_name(ErrCode c) {
    switch (c) {
#define X(en, name, human) case en: return name;
        ERRCODE_LIST(X)
#undef X
        default: return "UNKNOWN_CODE";
    }
}

const char* errcode_human(ErrCode c) {
    switch (c) {
#define X(en, name, human) case en: return human;
        ERRCODE_LIST(X)
#undef X
        default: return "Unknown error";
    }
}

int error_format_line(const ErrorEvent* ev, char* buf, size_t buf_sz) {
    if (!buf || !buf_sz) return 0;
    const char* msg = (ev->msg && ev->msg[0]) ? ev->msg : errcode_human(ev->code);
    return snprintf(buf, buf_sz,
        "[%s][u%u] src:%s code:%s msg:%s data:%ld t:%lu",
        errsev_name(ev->severity),
        (unsigned)ev->ctrl_id,
        errsrc_name(ev->source),
        errcode_name(ev->code),
        msg,
        (long)ev->data,
        (unsigned long)ev->ts_ms
    );
}
