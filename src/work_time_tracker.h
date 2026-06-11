// WorkTimeTracker — накопительный счётчик времени работы устройства.
//
// До этого `workTimeCounter` в info publish был равен `millis()/1000` —
// uptime текущего boot'а. Любой ребут (включая OTA) обнулял его до 0.
// Это плохо для статистики службы (общее наработанное время устройства).
//
// Tracker читает накопленные секунды из NVS при `begin()`, отсчитывает
// сессионный uptime через `millis()`, периодически persistит сумму в NVS
// каждые PERSIST_INTERVAL_MS (5 мин — flash-friendly).
//
// Точность:
//   - Обычный crash/power-loss: ≤ PERSIST_INTERVAL_MS секунд потери.
//   - OTA reboot: 0 потери — OtaReceiver зовёт flush() перед ESP.restart().
//
// Flash wear: 1 запись в 5 мин = 288/день = ~105K записей/год. ESP32 NVS
// guarantees ~100K циклов на сектор, но wear-leveling internal API
// распределяет записи по доступным секторам в namespace. Реально безопасно.

#pragma once

#include <stdint.h>

namespace idryer {

class WorkTimeTracker {
public:
    /// Singleton — один tracker на устройство. SDK дёргает из Link::begin/loop
    /// и FacadeProfile::buildInfoJson, OtaReceiver::handleChunk перед restart.
    static WorkTimeTracker& instance();

    /// Прочитать накопленное значение из NVS. Запомнить `millis()` как
    /// начало текущей сессии. Вызывать один раз из Link::begin().
    void begin();

    /// Если прошло ≥ PERSIST_INTERVAL_MS с последнего persist — записать
    /// текущий total в NVS. Иначе no-op. Вызывать из Link::loop().
    void loop();

    /// Текущая сумма: baseSec_ (из NVS) + uptime текущей сессии в секундах.
    uint32_t total() const;

    /// Принудительно записать текущий total в NVS прямо сейчас. Используется
    /// OtaReceiver перед ESP.restart() — гарантирует что секунды до OTA
    /// не теряются. Безопасно вызывать многократно.
    void flush();

private:
    void persist(uint32_t totalSec);

    uint32_t baseSec_ = 0;          // загружено из NVS
    uint32_t sessionStartMs_ = 0;   // millis() в момент begin() или последнего persist
    uint32_t lastPersistMs_ = 0;

    // 5 минут — компромисс между точностью (max 5 мин потери при power-loss)
    // и flash-wear (~105K записей/год при 24/7 работе, под лимитом ESP32 NVS).
    static constexpr uint32_t PERSIST_INTERVAL_MS = 5u * 60u * 1000u;

    WorkTimeTracker() = default;
    WorkTimeTracker(const WorkTimeTracker&) = delete;
    WorkTimeTracker& operator=(const WorkTimeTracker&) = delete;
};

} // namespace idryer
