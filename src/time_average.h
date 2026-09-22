// TimeAverage — среднее значения по времени за два последних окна.
//
// Значение, которое держится между редкими обновлениями (мощность ПИД,
// снимок из UART раз в 5 с), в точке публикации даёт случайный кадр.
// TimeAverage копит интеграл значения по времени: add() на каждом проходе
// loop(), rotate() закрывает окно — обычно в момент публикации. value()
// возвращает среднее за закрытое и текущее окно, то есть за два периода
// публикации: каждая точка перекрывает соседнюю наполовину, линия ровнее,
// а смена режима не ждёт — владелец зовёт reset().
//
// Один экземпляр — одна величина одного юнита. Кучи нет, 16 байт.

#pragma once

#include <stdint.h>

namespace idryer {

class TimeAverage {
public:
    /// Значение v держалось dtMs миллисекунд.
    void add(float v, uint32_t dtMs) {
        if (v != v) return;  // NAN — нет данных, в среднее не идёт
        sumCur_ += v * (float)dtMs;
        msCur_  += dtMs;
    }

    /// Среднее за закрытое и текущее окно; fallback — пока окна пусты.
    float value(float fallback) const {
        const uint32_t ms = msPrev_ + msCur_;
        return ms ? (sumPrev_ + sumCur_) / (float)ms : fallback;
    }

    /// Закрыть текущее окно: оно становится прошлым, прошлое уходит.
    void rotate() {
        sumPrev_ = sumCur_;
        msPrev_  = msCur_;
        sumCur_  = 0.0f;
        msCur_   = 0;
    }

    /// Забыть всё накопленное.
    void reset() {
        sumPrev_ = sumCur_ = 0.0f;
        msPrev_  = msCur_  = 0;
    }

    /// Длина текущего окна, мс.
    uint32_t currentMs() const { return msCur_; }

private:
    float    sumCur_  = 0.0f;
    float    sumPrev_ = 0.0f;
    uint32_t msCur_   = 0;
    uint32_t msPrev_  = 0;
};

}  // namespace idryer
