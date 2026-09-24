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

// ---------- Critical sections ----------
// Защита выбирается по платформе: на RP2040 нужен critical_section из Pico SDK
// (два ядра, запрета прерываний мало), на одноядерных Arduino-платформах
// хватает noInterrupts(), на голом Cortex-M — PRIMASK.
//
// Обёрнуто в функции, а не в макросы-выражения. noInterrupts() на Xtensa
// (ESP32-S3) разворачивается в do{...}while(0) — это инструкция, и внутрь
// выражения вида (noInterrupts(), 0) она не встаёт. На RISC-V (ESP32-C3) тот
// же макрос разворачивается в вызов функции, поэтому расхождение проявлялось
// только на S3: portmacro.h порта xtensa против порта riscv.
typedef uint32_t eb_crit_token_t;

#if defined(ARDUINO_ARCH_RP2040)
#include "pico/critical_section.h"
static critical_section_t eb_cs;
static bool eb_cs_inited = false;

static inline void eb_init_cs_once(void) {
  if (!eb_cs_inited) {
    critical_section_init(&eb_cs);
    eb_cs_inited = true;
  }
}
static inline eb_crit_token_t eb_crit_enter(void) {
  critical_section_enter_blocking(&eb_cs);
  return 0;
}
static inline void eb_crit_exit(eb_crit_token_t) { critical_section_exit(&eb_cs); }

#elif defined(ARDUINO)
#include <Arduino.h>

static inline void eb_init_cs_once(void) { /* no-op */ }
static inline eb_crit_token_t eb_crit_enter(void) {
  noInterrupts();
  return 0;
}
static inline void eb_crit_exit(eb_crit_token_t) { interrupts(); }

#else
// CMSIS (Cortex-M) вариант: сохраняем PRIMASK, чтобы не включить прерывания,
// если они были выключены до входа.
#include "cmsis_gcc.h"

static inline void eb_init_cs_once(void) { /* no-op */ }
static inline eb_crit_token_t eb_crit_enter(void) {
  const eb_crit_token_t saved = __get_PRIMASK();
  __disable_irq();
  return saved;
}
static inline void eb_crit_exit(eb_crit_token_t saved) {
  if (!saved) __enable_irq();
}
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
  const eb_crit_token_t _t = eb_crit_enter();
  head_ = 0;
  tail_ = 0;
  eb_crit_exit(_t);
}

void errorbus_clear(void) {
  const eb_crit_token_t _t = eb_crit_enter();
  head_ = 0;
  tail_ = 0;
  eb_crit_exit(_t);
}

bool errorbus_post(const ErrorEvent *e) {
  const eb_crit_token_t _t = eb_crit_enter();
  uint8_t n = inc_(head_);
  if (n == tail_) { // the queue is full
    eb_crit_exit(_t);
    return false;
  }
  q_[head_] = *e; // copy event
  head_ = n;      // публикуем новую границу
  eb_crit_exit(_t);
  return true;
}

bool errorbus_poll(ErrorEvent *out) {
  const eb_crit_token_t _t = eb_crit_enter();
  if (tail_ == head_) { // пусто
    eb_crit_exit(_t);
    return false;
  }
  *out = q_[tail_];    // копируем наружу
  tail_ = inc_(tail_); // сдвигаем хвост
  eb_crit_exit(_t);
  return true;
}

uint8_t errorbus_count(void) {
  const eb_crit_token_t _t = eb_crit_enter();
  uint8_t h = head_;
  uint8_t t = tail_;
  eb_crit_exit(_t);
  return (uint8_t)((h + ERRORBUS_SIZE - t) % ERRORBUS_SIZE);
}