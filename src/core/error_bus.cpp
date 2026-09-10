// Кольцевая очередь событий ошибок. Одна реализация на все устройства:
// защита критической секции выбирается по платформе — на RP2040 нужен
// critical_section из Pico SDK (два ядра, запрета прерываний мало), на
// одноядерных Arduino-платформах хватает noInterrupts().
#include "error_bus.h"

// #define KASYAK_FINDER 1
// #define LOG_LEVEL LOG_LEVEL_ERRORBUS
// #define LOG_TAG "ERRORBUS"
// #include "debug_log.h"


#if (ERRORBUS_SIZE < 2)
#error "ERRORBUS_SIZE must be >= 2"
#endif

// ---------- Critical sections (RP2040-friendly) ----------
#if defined(ARDUINO_ARCH_RP2040)
// Двухъядерная защита с Pico SDK
#include "pico/critical_section.h"
static critical_section_t eb_cs;
static bool eb_cs_inited = false;

#define EB_CRIT_TOKEN int
#define EB_CRIT_ENTER() (critical_section_enter_blocking(&eb_cs), 0)
#define EB_CRIT_EXIT(_t) critical_section_exit(&eb_cs)

static inline void eb_init_cs_once(void) {
  if (!eb_cs_inited) {
    critical_section_init(&eb_cs);
    eb_cs_inited = true;
  }
}

#else
// Fallback path: interrupt blocking (single-core)
#if defined(ARDUINO)
#include <Arduino.h>
#define EB_CRIT_TOKEN uint8_t
#define EB_CRIT_ENTER() (noInterrupts(), 0)
#define EB_CRIT_EXIT(_t) interrupts()
#else
// CMSIS (Cortex-M) вариант
#include "cmsis_gcc.h"
#define EB_CRIT_TOKEN uint32_t
#define EB_CRIT_ENTER()                                                                                                                                                                                                                                                                                                                                                                                        \
  __get_PRIMASK();                                                                                                                                                                                                                                                                                                                                                                                             \
  __disable_irq()
#define EB_CRIT_EXIT(s)                                                                                                                                                                                                                                                                                                                                                                                        \
  do {                                                                                                                                                                                                                                                                                                                                                                                                         \
    if (!(s)) __enable_irq();                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)
#endif

static inline void eb_init_cs_once(void) { /* no-op */ }
#endif
// -----------------------------------------------------------

static ErrorEvent q_[ERRORBUS_SIZE];
static volatile uint8_t head_ = 0, tail_ = 0;

static inline uint8_t inc_(uint8_t x) {
  // Если ERRORBUS_SIZE — степень 2 (8/16/32/...), можно ускорить:
  // return (uint8_t)((x + 1) & (ERRORBUS_SIZE - 1));
  return (uint8_t)((x + 1) % ERRORBUS_SIZE);
}

void errorbus_init(void) {
  eb_init_cs_once();
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

bool errorbus_post(const ErrorEvent *e) {
  EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
  uint8_t n = inc_(head_);
  if (n == tail_) { // the queue is full
    EB_CRIT_EXIT(_t);
    return false;
  }
  q_[head_] = *e; // copy event
  head_ = n;      // публикуем новую границу
  EB_CRIT_EXIT(_t);
  return true;
}

bool errorbus_poll(ErrorEvent *out) {
  EB_CRIT_TOKEN _t = EB_CRIT_ENTER();
  if (tail_ == head_) { // пусто
    EB_CRIT_EXIT(_t);
    return false;
  }
  *out = q_[tail_];    // копируем наружу
  tail_ = inc_(tail_); // сдвигаем хвост
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