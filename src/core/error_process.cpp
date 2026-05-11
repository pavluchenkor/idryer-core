#include "error_process.h"

static ErrorCallback g_cb        = nullptr;
static uint8_t       g_ctrl_mask = 0xFF;           // bits 0..7 = ctrl_ids 0..7
static ErrSeverity   g_min_sev   = ERRSEV_INFO;    // pass all by default

void error_set_handler(ErrorCallback cb)          { g_cb = cb; }
void error_set_min_severity(ErrSeverity min_sev)  { g_min_sev = min_sev; }
void error_enable_ctrl(uint8_t id)                { if (id < 8) g_ctrl_mask |=  (1u << id); }
void error_disable_ctrl(uint8_t id)               { if (id < 8) g_ctrl_mask &= ~(1u << id); }

void error_process_all(void) {
    if (!g_cb) return;
    ErrorEvent ev;
    while (errorbus_poll(&ev)) {
        if (ev.severity < g_min_sev)                    continue;
        if (!(g_ctrl_mask & (1u << (ev.ctrl_id & 7)))) continue;
        g_cb(&ev);
    }
}
