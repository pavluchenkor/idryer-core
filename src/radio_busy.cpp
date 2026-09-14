#include "radio_busy.h"

namespace idryer {

namespace {
// volatile: ставится и снимается в основном цикле, читается в том числе из
// кода, который вызывается по событиям стека.
volatile uint8_t s_busyDepth = 0;
}

void radioBusyBegin() {
    if (s_busyDepth < 255) s_busyDepth++;
}

void radioBusyEnd() {
    if (s_busyDepth > 0) s_busyDepth--;
}

bool radioBusy() { return s_busyDepth != 0; }

} // namespace idryer
