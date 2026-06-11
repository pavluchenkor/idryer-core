// WorkTimeTracker implementation — см. work_time_tracker.h для контракта.
//
// ESP-only: использует Preferences (NVS). На RP2040 (RP-сторона DRYER) этот
// модуль не нужен — WTC там живёт в EEPROM через menu-binding.

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "work_time_tracker.h"
#include "hal/hal_types.h"

#include <Arduino.h>
#include <Preferences.h>

namespace idryer {

namespace {
constexpr const char* NVS_NS  = "idryer-wtc"; // отдельный namespace, не пересекается с меню
constexpr const char* KEY_TOTAL = "total_sec";
} // namespace

WorkTimeTracker& WorkTimeTracker::instance() {
    static WorkTimeTracker s_instance;
    return s_instance;
}

void WorkTimeTracker::begin() {
    Preferences prefs;
    if (prefs.begin(NVS_NS, /*readOnly=*/true)) {
        baseSec_ = prefs.getUInt(KEY_TOTAL, 0);
        prefs.end();
        HAL_LOG_INFO("WTC", "loaded base=%u sec from NVS", (unsigned)baseSec_);
    } else {
        HAL_LOG_WARN("WTC", "NVS open failed, starting from 0");
        baseSec_ = 0;
    }
    sessionStartMs_ = millis();
    lastPersistMs_  = sessionStartMs_;
}

uint32_t WorkTimeTracker::total() const {
    uint32_t sessionSec = (millis() - sessionStartMs_) / 1000u;
    return baseSec_ + sessionSec;
}

void WorkTimeTracker::loop() {
    uint32_t now = millis();
    if (now - lastPersistMs_ < PERSIST_INTERVAL_MS) return;
    persist(total());
    lastPersistMs_ = millis(); // не now: persist может занять немного времени
}

void WorkTimeTracker::flush() {
    persist(total());
    lastPersistMs_ = millis();
}

void WorkTimeTracker::persist(uint32_t totalSec) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, /*readOnly=*/false)) {
        HAL_LOG_ERROR("WTC", "NVS open RW failed — persist skipped");
        return;
    }
    prefs.putUInt(KEY_TOTAL, totalSec);
    prefs.end();
    // Сдвигаем base + сбрасываем session: следующий total() будет
    //   baseSec_(новый) + (millis - sessionStart)/1000
    // что даст ту же сумму, но избегает повторного учёта уже сохранённых секунд.
    baseSec_ = totalSec;
    sessionStartMs_ = millis();
    HAL_LOG_DEBUG("WTC", "persisted total=%u sec", (unsigned)totalSec);
}

} // namespace idryer

#endif // ESP32 / ESP_PLATFORM
