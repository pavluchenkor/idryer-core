#include "error_bus.h"

#if (ERRORBUS_SIZE < 2)
#error "ERRORBUS_SIZE must be >= 2"
#endif

// Arduino (ESP32): disable/enable interrupts for atomic access
#if defined(ARDUINO)
#include <Arduino.h>
static inline uint8_t eb_crit_enter_() { noInterrupts(); return 0; }
#define EB_CRIT_TOKEN    uint8_t
#define EB_CRIT_ENTER()  eb_crit_enter_()
#define EB_CRIT_EXIT(_t) interrupts()
#else
// Fallback for unit tests / host builds — no-op
#define EB_CRIT_TOKEN   int
#define EB_CRIT_ENTER() 0
#define EB_CRIT_EXIT(_t) (void)(_t)
#endif

static ErrorEvent       q_[ERRORBUS_SIZE];
static volatile uint8_t head_ = 0, tail_ = 0;

static inline uint8_t inc_(uint8_t x) {
    return (uint8_t)((x + 1) % ERRORBUS_SIZE);
}

void errorbus_init(void) {
    EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
    head_ = 0;
    tail_ = 0;
    EB_CRIT_EXIT(_t);
}

void errorbus_clear(void) {
    EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
    head_ = 0;
    tail_ = 0;
    EB_CRIT_EXIT(_t);
}

bool errorbus_post(const ErrorEvent* e) {
    EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
    uint8_t n = inc_(head_);
    if (n == tail_) {
        EB_CRIT_EXIT(_t);
        return false;   // queue full — event dropped
    }
    q_[head_] = *e;
    head_ = n;
    EB_CRIT_EXIT(_t);
    return true;
}

bool errorbus_poll(ErrorEvent* out) {
    EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
    if (tail_ == head_) {
        EB_CRIT_EXIT(_t);
        return false;   // queue empty
    }
    *out = q_[tail_];
    tail_ = inc_(tail_);
    EB_CRIT_EXIT(_t);
    return true;
}

uint8_t errorbus_count(void) {
    EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
    uint8_t h = head_;
    uint8_t t = tail_;
    EB_CRIT_EXIT(_t);
    return (uint8_t)((h + ERRORBUS_SIZE - t) % ERRORBUS_SIZE);
}
